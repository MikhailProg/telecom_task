#ifndef UTILS_H
#define UTILS_H

#define ARRSZ(a)		(sizeof(a) / sizeof((a)[0]))

int fd_check_sock_connection(int fd);
int fd_nonblock(int fd);

#endif
