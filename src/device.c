#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <err.h>

#include "utils.h"
#include "loop.h"
#include "unix.h"
#include "list.h"
#include "device.h"

#define PEER_BUF_SIZE	128

#define MSG_HELLO	1
#define MSG_SET		2
#define MSG_GET		3
#define MSG_RES		4

#define	PARAM_TEXT	1	/* nul-terminated */
#define PARAM_TEMP	2	/* 2 bytes */
#define PARAM_BRGHT	3	/* 2 bytes */
#define PARAM_MAX	4

enum {
	DEV_STATE_UNKNOWN = 0,
	DEV_STATE_CONTROLLER,
	DEV_STATE_MASTER,
	DEV_STATE_SLAVE,
};

struct peer {
	struct list	list;
	Device		*dev;	/* point to its device */
	int		fd;
	unsigned char	*buf;
	size_t		size;	/* buf size */
	size_t		off;	/* offset in buf for rd/wr */
	size_t		left;	/* left bytes for wr */
	int		(*on_in)(Peer *p);
	int		(*on_out)(Peer *p);
	void		(*on_drop)(Peer *p, int eof);
};

struct range {
	int from;
	int to;
};

typedef struct range Range;

static Peer *peer_alloc(int fd, Device *dev)
{
	Peer *p = calloc(1, sizeof(*p) + PEER_BUF_SIZE);
	if (p == NULL) {
		return NULL;
	}

	p->fd = fd;
	p->buf = (unsigned char *)p + sizeof(*p);
	p->size = PEER_BUF_SIZE;
	p->dev = dev;

	return p;	
}

static void peer_dealloc(Peer *p)
{
	free(p);
}

static void peer_close(Peer *p)
{
	loop_fd_del(p->fd);
	close(p->fd);
	peer_dealloc(p);
}

static const char *device_state2name(const Device *dev)
{
	switch (dev->state) {
	case DEV_STATE_MASTER:
		return "MASTER";
	case DEV_STATE_CONTROLLER:
		return "CONTROLLER";
	case DEV_STATE_SLAVE:
		return "SLAVE";
	case DEV_STATE_UNKNOWN:
		return "UNKNOWN";
	default:
		abort();
	}
}

static int device_is_controller(const Device *dev)
{
	return dev->state == DEV_STATE_CONTROLLER ? 1 : 0;
}

static int device_is_polling_inprogress(Device *dev)
{
	return dev->head ? 1 : 0;
}

static void device_drop_peers(Device *dev)
{
	list_foreach((struct list *)dev->head, (void *)peer_close, NULL);
	dev->head = NULL;
}

static uint16_t get_u16(uint8_t *p)
{
	return (p[0] << 8) | p[1];
}

static void put_u16(uint8_t *p, uint16_t n)
{
	p[0] = (n >> 8) & 0xff;
	p[1] = n        & 0xff;
}

static void peer_hello_req_send(Peer *p)
{
	*p->buf = MSG_HELLO;
	p->left = 1;
	p->off  = 0;
}

static int peer_hello_req_recv(Peer *p)
{
	if (p->off != 1) {
		return -1;
	}
	peer_hello_req_send(p);
	loop_fd_change(p->fd, LOOP_WR);
	return 0;
}

static int peer_get_req_recv(Peer *p)
{
	unsigned char *q = p->buf;
	/* Generate random values in response. */
	struct {
		uint8_t	 n;
		uint16_t v;
	} val[] = {
		{ MSG_RES,	9 },
#define GEN_XXX(s, e)	((s) + rand() % ((e) - (s)) + 1)
		{ PARAM_TEMP,	GEN_XXX(10, 25) },
		{ PARAM_BRGHT,	GEN_XXX(50, 70) }
	};
#undef GEN_XXX
	int i;
	
	warnx("RECV GET as slave");

	for (i = 0; i < (int)ARRSZ(val); i++) {
		*q++ = val[i].n;
		put_u16(q, val[i].v);
		q += 2;
	}

	p->off  = 0;
	p->left = 9;

	loop_fd_change(p->fd, LOOP_WR);
	return 0;
}

static int peer_set_req_recv(Peer *p)
{
	Device *dev = p->dev;
	unsigned char *q = p->buf;
	size_t n = p->off;
	
	struct {
		union {
			uint16_t val;
			char	 *str;
		};
		int	 upd;
	} params[PARAM_MAX];

	/* Partial read. */
	if (n < 3) {
		return 1;
	}

	q += 1;
	uint16_t len = get_u16(q);
	/* read more bytes than packet reports or len is more than buf max size */
	if (n > len || len > p->size) {
		return -1;
	}
	q += 2;

	/* Partial read. */
	if (len > n) {
		return 1;
	}

	assert(n == len);
	
	memset(&params, 0, sizeof(params));
	unsigned char *e = q + len - 3;

	while (q < e) {
		unsigned char type = *q++;
		unsigned char *s;
		switch (type) {
		case PARAM_TEXT:
			s = memchr(q, 0, e - q);
			if (s == NULL) {
				return -1;
			}
			params[type].upd = 1;
			params[type].str = (char *)q;
			q = s + 1;
			break;
		case PARAM_BRGHT:
			if (q + 2 > e) {
				return -1;
			}
			params[type].upd = 1;
			params[type].val = get_u16(q);
			q += 2;
			break;
		default:
			return -1;
		}
	}

	if (!params[PARAM_TEXT].upd || !params[PARAM_BRGHT].upd) {
		return -1;
	}

	warnx("RECV SET as slave: brigtness: %u, message: \"%s\"",
			params[PARAM_BRGHT].val, params[PARAM_TEXT].str);
	dev->display(dev, " [ %s %u ] brigtness: %u, message: \"%s\"",
			device_state2name(dev) , dev->host,
			params[PARAM_BRGHT].val, params[PARAM_TEXT].str);
				
	peer_close(p);
	return 0;
}

static int peer_msg_req_recv(Peer *p)
{
	Device *dev = p->dev;
	unsigned char type = *p->buf;
	int rc = -1;

	switch (type) {
	case MSG_HELLO:
		rc = peer_hello_req_recv(p);
		break;
	case MSG_GET:
		rc = peer_get_req_recv(p);
		break;
	case MSG_SET:
		rc = peer_set_req_recv(p);
		break;
	default:
		rc = -1;
		break;
	}

	if (rc < 0) {
		return rc;	
	}
	
	/* HELLO is echoed and doesn't influence on the state. */
	if (type == MSG_HELLO) {
		return 0;
	}

	/* In unkown and master states the device can poll sensors, since someone
	 * polls us the device is a slave so stop polling. */
	if (device_is_polling_inprogress(dev)) {
		device_drop_peers(dev);
	}

	if (dev->state != DEV_STATE_SLAVE) {
		warnx("%s -> SLAVE", device_state2name(dev));
		dev->state = DEV_STATE_SLAVE;
		dev->display(dev, " [ %s %u ]", device_state2name(dev), dev->host);
	}

	assert(dev->state == DEV_STATE_SLAVE);
	device_next_step(dev);

	return 0;
}

static void peer_rdwr_event(int fd, LoopEvent event, void *opaque)
{
	Peer *p = opaque;
	int eof = 0;

	if (event & LOOP_ERR) {
		goto drop;
	}

	if (event & LOOP_RD) {
		ssize_t n = recv(fd, p->buf + p->off, p->size - p->off, 0);
		if (n <= 0) {
			if (n == 0 || (n < 0 && !SOFT_ERROR)) {
				eof = n == 0 ? 1 : 0;
				goto drop;
			}
		} else {
			p->off += n;
			if (p->on_in(p) < 0) {
				goto drop;
			}
		}
	} else if (event & LOOP_WR) {
		ssize_t n = send(fd, p->buf + p->off, p->left, 0);
		if (n < 0) {
			if (n < 0 && !SOFT_ERROR) {
				goto drop;
			}
		} else {
			p->off  += n;
			p->left -= n;
			if (!p->left) {
				if (p->on_out == NULL) {
					goto drop;
				}
				if (p->on_out(p) < 0) {
					goto drop;
				}
			}
		}
	}
	
	return;
	
drop:
	if (p->on_drop) {
		p->on_drop(p, eof);
	} else {
		peer_close(p);
	}
}

static int peer_close_after_write(Peer *p)
{
	peer_close(p);
	return 0;
}

static void device_srv_event(int fd, LoopEvent event, void *opaque)
{
	Device *dev = opaque;
	struct sockaddr_storage ss;
	socklen_t slen = sizeof(ss);

	if (!(event & LOOP_RD)) {
		return;
	}

	int afd = accept(fd, (void *)&ss, &slen);
	if (afd < 0) {
		if (!SOFT_ERROR) {
			warn("accept()");
		}
		return;
	}

	if (fd_nonblock(afd) < 0) {
		close(afd);
		return;
	}

	Peer *p = peer_alloc(afd, dev);
	if (p == NULL) {
		close(afd);
		return;
	}

	p->on_in = peer_msg_req_recv;
	p->on_out = peer_close_after_write;
	loop_fd_add(afd, LOOP_RD, peer_rdwr_event, p);
}

static int device_params_put(Device *dev, uint16_t temp, uint16_t brgth)
{
	Param param = { temp, brgth };

	if (dev->params_used == dev->params_size) {
		dev->params_size = dev->params_size ? 2 * dev->params_size : 8;
		Param *params = realloc(dev->params,
					sizeof(Param) * dev->params_size);
		if (params == NULL) {
			warnx("params realloc() failed");
			return -1;
		}
		dev->params = params;
	}

	dev->params[dev->params_used++] = param;
	return 0;
}

static int fd_check_conn(int fd)
{
	int err = 0;
	socklen_t elen = sizeof(err);

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen) < 0) {
		err = errno;
		warn("getsockopt");
	} else if (err) {
		errno = err;
		warn("connect (deferred)");
	} else {
#if 0
		warnx("fd=%d connected", fd);
#endif
	}

	return err ? 0 : 1;
}

static int peer_check_conn(const Peer *p)
{
	return fd_check_conn(p->fd);
}

static void peer_get_req_send(Peer *p)
{
	*p->buf = MSG_GET;
	p->left = 1;
	p->off  = 0;
}

static void peer_set_req_send(Peer *p, const char *msg,
				size_t n, uint16_t brght)
{
	unsigned char *q = p->buf;

	/* n must includes 0 */
	n = n > p->size - 7 ? p->size - 7 : n;
 	uint16_t len = 1 + 2 + 1 + n + 1 + 2;

	*q++ = MSG_SET;
	put_u16(q, len);
	q += 2;

	*q++ = PARAM_TEXT;
	memcpy(q, msg, n);
	q[n] = 0;	/* make sure it is nullterminated */
	q += n;

	*q++ = PARAM_BRGHT;
	put_u16(q, brght);
	q += 2;

	p->left = len;
	p->off  = 0;
}

static void device_master_or_slave_resolve(Device *dev)
{
	assert(dev->state != DEV_STATE_UNKNOWN);
	warnx("%u is %s", dev->host, device_state2name(dev));
	dev->display(dev, " [ %s %u ]", device_state2name(dev), dev->host);
	device_next_step(dev);
}

static int peer_hello_resp_recv(Peer *p)
{
	Device *dev = p->dev;

	assert(dev->state == DEV_STATE_UNKNOWN);

	/* Someone with a higher address answered then drop all
	 * other HELLO peers cause we don't need their results. */
	if (p->off == 1 && p->buf[0] == MSG_HELLO) {
		dev->state = DEV_STATE_SLAVE;
		device_drop_peers(dev);
		device_master_or_slave_resolve(dev);
	} else {
		list_remove((struct list **)&dev->head, (struct list *)p);
		peer_close(p);
		if (!device_is_polling_inprogress(dev)) {
			dev->state = DEV_STATE_MASTER;
			device_master_or_slave_resolve(dev);
		}
	}

	return 0;
}

static int peer_get_resp_recv(Peer *p)
{
	Device *dev = p->dev;
	unsigned char *q = p->buf;
	size_t n = p->off;
	
	struct {
		uint16_t val;
		int	 upd;
	} params[PARAM_MAX];

	if (*q != MSG_RES) {
		return -1;
	}

	//puts(__func__);
	q += 1;
	/* Partial read. */
	if (n < 3) {
		return 1;
	}

	uint16_t len = get_u16(q);
	if (len != 9) {
		return -1;
	}
	q += 2;

	/* Partial read. */
	if (len > n) {
		return 1;
	}

	assert(len == n);

	memset(&params, 0, sizeof(params));
	unsigned char *e = q + len - 3;
	
	while (q < e) {
		unsigned char type = *q++;
		switch (type) {
		case PARAM_TEMP:
		case PARAM_BRGHT:
			params[type].upd = 1;
			params[type].val = get_u16(q);
			q += 2;
			break;
		default:
			return -1;
		}
	}

	if (!params[PARAM_TEMP].upd || !params[PARAM_BRGHT].upd) {
		return -1;
	}

	device_params_put(dev, params[PARAM_TEMP].val, params[PARAM_BRGHT].val);

	list_remove((struct list **)&dev->head, (struct list *)p);
	peer_close(p);

	return 0;
}

static int peer_rd_after_wr(Peer *p)
{
	p->off = 0;
	loop_fd_change(p->fd, LOOP_RD);
	return 0;
}

static void peer_on_drop(Peer *p, int eof)
{
	Device *dev = p->dev;
	(void)eof;

	list_remove((struct list **)&dev->head, (struct list *)p);
	peer_close(p);

}

static void
device_connect_range(Device *dev, const Range *range, int excl,
			void on_connect(int fd, LoopEvent event, void *opaque))
{
	int i;

	for (i = range->from; i <= range->to; i++) {
		if (excl >= 0 && excl == i) {
			continue;
		}
		char sock[32];
		snprintf(sock, sizeof(sock), "%d", i);

		int fd = unix_connect(sock, 1);
		if (fd < 0) {
			continue;
		}

		Peer *p = peer_alloc(fd, dev);
		if (p == NULL) {
			close(fd);
			continue;
		}

		/* Add special handler to remove from list in case of failure. */
		p->on_drop = peer_on_drop;
		list_prepend((struct list **)&dev->head, (struct list *)p);
		
		loop_fd_add(p->fd, LOOP_WR, on_connect, p);
	}
}

static void
peer_master_or_slave_on_connect(int fd, LoopEvent event, void *opaque)
{
	Peer *p = opaque;
	Device *dev = p->dev;

	assert(dev->state == DEV_STATE_UNKNOWN);
	(void)event;

	/* If connection was success send hello request. */
	if (peer_check_conn(p)) {
		loop_fd_del(fd);
		p->on_in  = peer_hello_resp_recv;
		p->on_out = peer_rd_after_wr;
		peer_hello_req_send(p);
		loop_fd_add(p->fd, LOOP_WR, peer_rdwr_event, p);
	} else {
		list_remove((struct list **)&dev->head, (struct list *)p);
		peer_close(p);
		if (!device_is_polling_inprogress(dev)) {
			dev->state = DEV_STATE_MASTER;
			device_master_or_slave_resolve(dev);
		}
	}
}

static void device_master_or_slave(Device *dev)
{
	/* Connect to addresses that are greater to detect the device role. */
	const Range range = {
		dev->host + 1, DEVICE_HOST_MAX
	};

	device_connect_range(dev, &range, -1, peer_master_or_slave_on_connect);

	/* There is no peers left to poll, so the device is a master. */
	if (!device_is_polling_inprogress(dev)) {
		dev->state = DEV_STATE_MASTER;
		device_master_or_slave_resolve(dev);
	}
}

static void peer_poll(Peer *p)
{
	Device *dev = p->dev;

	/* send set request every 3 cycles */
	if (dev->poll_cycles % 3 == 0) {
		peer_set_req_send(p, dev->net_msg, dev->net_msg_len, 
					dev->param_avg.brgth);
	} else {
		peer_get_req_send(p);
	}
}

static void peer_poll_on_connect(int fd, LoopEvent event, void *opaque)
{
	Peer *p = opaque;
	Device *dev = p->dev;

	(void)event;

	if (peer_check_conn(p)) {
		loop_fd_del(fd);
		p->on_in  = peer_get_resp_recv;
		p->on_out = peer_rd_after_wr;
		peer_poll(p);
		loop_fd_add(p->fd, LOOP_WR, peer_rdwr_event, p);
	} else {
		list_remove((struct list **)&dev->head, (struct list *)p);
		peer_close(p);
	}
}

static void device_net_msg_set(Device *dev)
{
	char date[128];
	struct tm *tm; 
	time_t t; 
	
	time(&t); 
	tm = localtime(&t); 

	strftime(date, sizeof(date), "%a %b %d %R", tm);
	dev->net_msg_len = 1 + snprintf(dev->net_msg, sizeof(dev->net_msg),
					"%u'C, %s", dev->param_avg.temp, date);
}

static void device_param_avg_calc(Device *dev)
{
	if (!dev->params_used) {
		return;
	}

	uint64_t t = 0, b = 0;
	size_t i;

	for (i = 0; i < dev->params_used; i++) {
		t += dev->params[i].temp;
		b += dev->params[i].brgth;
	}

	dev->param_avg.temp  = t / dev->params_used;
	dev->param_avg.brgth = b / dev->params_used;
	dev->params_used = 0;

	device_net_msg_set(dev);
	warnx("CALC");
	dev->display(dev, " [ %s %u ] brigtness (avg): %u, temp (avg): %u'C",
			device_state2name(dev) , dev->host,
			dev->param_avg.brgth, dev->param_avg.temp);
}

static void device_poll_sensors(Device *dev)
{
	/* If polling is in progress it means the previous poll is not finished
	 * due to slow or unreacheable peers. Drop unfinished peers. */
	if (device_is_polling_inprogress(dev)) {
		device_drop_peers(dev);
	}

	++dev->poll_cycles;
	/* Calculate averages from the previous cycle. */
	device_param_avg_calc(dev);
	
	/* The controller polls all hosts exluding itself.
	 * The master polls hosts which addresses are less. */
	Range range = {
		1, device_is_controller(dev) ? DEVICE_HOST_MAX : dev->host - 1
	};
	int excl = device_is_controller(dev) ? dev->host : -1;

	device_connect_range(dev, &range, excl, peer_poll_on_connect);

	/* Schedule a new polling. */
	dev->resched_timer(dev, DEVICE_MASTER_TIMEOUT);
}

void device_next_step(Device *dev)
{
	switch (dev->state) {
	case DEV_STATE_UNKNOWN:
		dev->params_used = 0;
		dev->resched_timer(dev, 0);
		device_master_or_slave(dev);
		break;
	case DEV_STATE_SLAVE:
		dev->params_used = 0;
		dev->resched_timer(dev, DEVICE_SLAVE_TIMEOUT);
		break;
	case DEV_STATE_CONTROLLER:
	case DEV_STATE_MASTER:
		device_poll_sensors(dev);
		break;
	default:
		abort();
	}
}

static void device_on_timeout(Device *dev)
{
	switch (dev->state) {
	case DEV_STATE_SLAVE:
		/* There are no requests for a long time. */
		dev->state = DEV_STATE_UNKNOWN;
		device_next_step(dev);
		break;
	case DEV_STATE_CONTROLLER:
	case DEV_STATE_MASTER:
		/* Rerun sensors polling. */
		device_next_step(dev);
		break;
	default:
		abort();
	}
}	

int device_init(Device *dev, int host, int is_controller)
{
	memset(dev, 0, sizeof(*dev));
	dev->state = is_controller ? DEV_STATE_CONTROLLER : DEV_STATE_UNKNOWN;
	dev->host = host;
	dev->on_timeout = device_on_timeout;
	dev->fd = -1;

	if (!is_controller) {
		char sock[32];
		snprintf(sock, sizeof(sock), "%d", host);

		int rc = unlink(sock);
		if (rc < 0 && errno != ENOENT) {
			warn("unlink()");
			return -1;
		}

		int fd = unix_listen(sock);
		if (fd < 0) {
			warn("unix_listen()");
			return -1;
		}

		dev->fd = fd;
		loop_fd_add(fd, LOOP_RD, device_srv_event, dev);
	}

	return 0;
}

void device_deinit(Device *dev)
{
	if (dev->fd != -1) {
		loop_fd_del(dev->fd);
		close(dev->fd);
	}

	char sock[32];
	snprintf(sock, sizeof(sock), "%d", dev->host);
	unlink(sock);

	device_drop_peers(dev);
	free(dev->params);
}
