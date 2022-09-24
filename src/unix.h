#ifndef UNIX_H
#define UNIX_H

int unix_listen(const char *path);
int unix_connect(const char *path, int nonblock);

#endif
