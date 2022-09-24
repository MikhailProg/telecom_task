 #include <sys/socket.h>
 #include <sys/un.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "utils.h"
#include "unix.h"

int unix_listen(const char *path)
{
	struct sockaddr_un sun;
	int un, rc;
	size_t n;

	n = strlen(path);
	if (n >= sizeof(sun.sun_path)) {
		errno = EINVAL;
		return -1;
	}

	un = socket(AF_UNIX, SOCK_STREAM, 0);
	if (un < 0)
		return -1;

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, path, n + 1);

	rc = bind(un, (void *)&sun, sizeof(sun));
	if (rc < 0) {
		close(un);
		return -1;
	}

	rc = listen(un, SOMAXCONN);
	if (rc < 0) {
		close(un);
		return -1;
	}

	return un;
}

int unix_connect(const char *path, int nonblock)
{
	struct sockaddr_un sun;
	int un, rc;
	size_t n;

	n = strlen(path);
	if (n >= sizeof(sun.sun_path)) {
		errno = EINVAL;
		return -1;
	}

	un = socket(AF_UNIX, SOCK_STREAM, 0);
	if (un < 0)
		return -1;

	if (nonblock && fd_nonblock(un) < 0) {
		close(un);
		return -1;
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, path, n + 1);

	rc = connect(un, (void *)&sun, sizeof(sun));
	if (rc < 0) {
		if (!nonblock || (nonblock && errno != EINPROGRESS)) {
			close(un);
			return -1;
		}
	}

	return un;
}


