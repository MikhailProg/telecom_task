#ifndef UNIX_H
#define UNIX_H

int unix_listen(const char *path);
int unix_accept(int un, int nonblock);
int unix_connect(const char *path, int nonblock);
int unix_check_connection(int un);

#endif
