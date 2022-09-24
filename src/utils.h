#ifndef UTILS_H
#define UTILS_H

#define ARRSZ(a)		(sizeof(a) / sizeof((a)[0]))
#define SOFT_ERROR		(errno == EINTR || errno == EAGAIN || \
				 errno == EWOULDBLOCK)

int fd_nonblock(int fd);

#endif
