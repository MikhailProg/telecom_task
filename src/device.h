#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>

/* Timeout to polling sensors. */
#define	DEVICE_MASTER_TIMEOUT	2
/* Timeout for waiting a request from a controller. */
#define DEVICE_SLAVE_TIMEOUT	(3 * DEVICE_MASTER_TIMEOUT)	
#define DEVICE_HOST_MAX		256

typedef struct peer Peer;

struct param {
	uint16_t	temp;
	uint16_t	brgth;
};

typedef struct param Param;
typedef struct device Device;

struct device {
	int	state;
	int	host;		/* host addr */
	int	fd;		/* srv fd to accept connection */
	Peer	*head;		/* list of polling devices */
	Param	*params;	/* immediate params from sensors */
	size_t	params_used;
	size_t	params_size;
	Param	param_avg;	/* calucated avg params for sending */
	char	net_msg[64];	/* master message to send to other devices */
	int	net_msg_len;	/* cached net_msg length */
	int	poll_cycles;

	void	(*display)(Device *dev, const char *fmt, ...)
				__attribute__ ((format (printf, 2, 3)));
	void	(*resched_timer)(Device *dev, int secs);
	void	(*on_timeout)(Device *dev);
};


int device_init(Device *dev, int host, int is_controller);

void device_deinit(Device *dev);

void device_next_step(Device *dev);

#endif 
