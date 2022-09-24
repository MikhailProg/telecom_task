#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "utils.h"

int fd_check_sock_connection(int fd)
{
	int err = 0;
	socklen_t elen = sizeof(err);

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen) < 0) {
		err = errno;
	} else if (err) {
		errno = err;
	}

	return err ? 0 : 1;
}

int fd_nonblock(int fd)
{
	int flags;
	return ((flags = fcntl(fd, F_GETFL)) < 0 ||
			 fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
				? -1 : 0;
}


