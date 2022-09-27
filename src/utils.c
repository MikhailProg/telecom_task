#include <fcntl.h>

#include "utils.h"

int fd_nonblock(int fd)
{
	int flags;
	return ((flags = fcntl(fd, F_GETFL)) < 0 ||
			 fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
				? -1 : 0;
}

