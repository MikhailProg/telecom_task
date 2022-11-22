#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>

/* Timeout to polling sensors in msec. */
#define	DEVICE_MASTER_TIMEOUT	2000
/* Timeout for waiting a request from a controller. */
#define DEVICE_SLAVE_TIMEOUT	(3 * DEVICE_MASTER_TIMEOUT)
#define DEVICE_HOST_ADDR_MAX	255

typedef struct Peer Peer;
typedef struct Param Param;
typedef struct Device Device;
typedef struct DeviceOps DeviceOps;

struct Param {
	uint16_t	temp;
	uint16_t	brgth;
};

struct DeviceOps {
	void	(*display)(const Device *dev, const char *fmt, ...)
				__attribute__ ((format (printf, 2, 3)));
	void	(*timer)(const Device *dev, int msec);
};

struct Device {
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
	const DeviceOps *ops;
};


int device_init(Device *dev, int host, int iscontroller, const DeviceOps *ops);

void device_run(Device *dev);

void device_timeout(Device *dev);

void device_deinit(Device *dev);

#endif
