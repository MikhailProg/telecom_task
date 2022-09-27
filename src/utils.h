#ifndef UTILS_H
#define UTILS_H

#define ARRSZ(a)	(sizeof(a) / sizeof((a)[0]))

int fd_nonblock(int fd);

#endif
