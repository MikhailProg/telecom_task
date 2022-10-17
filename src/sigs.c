#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "loop.h"
#include "utils.h"
#include "sigs.h"

static int sigpipe[2];
static sig_atomic_t signals[NSIG];
static void (*signals_notify)(sigset_t *sigmask);

static void sig_event(int fd, LoopEvent event, void *opaque)
{
	sigset_t mask, oldmask, sigmask;
	char buf[32];
	int i, rc;

	UNUSED(opaque);
	UNUSED(event);

	/* Drain all data from event fd till EAGAIN. */
	do {
		rc = read(fd, buf, sizeof(buf));
	} while (rc > 0 || (rc < 0 && errno == EINTR));

	sigfillset(&mask);
	sigprocmask(SIG_BLOCK, &mask, &oldmask);

	sigemptyset(&sigmask);
	for (i = 1; i < NSIG; i++) {
		if (signals[i]) {
			sigaddset(&sigmask, i);
			signals[i] = 0;
		}
	}

	sigprocmask(SIG_SETMASK, &oldmask, NULL);

	if (signals_notify) {
		signals_notify(&sigmask);
	}
}

static void sigall(int signo)
{
	unsigned char a = 42;
	int rc, save_errno = errno;

	do {
		rc = write(sigpipe[1], &a, 1);
	} while (rc < 0 && errno == EINTR);

	signals[signo] = 1;
	errno = save_errno;
}

int sigs_init(void (*notify)(sigset_t *sigmask))
{
	struct sigaction sa;
	int i;

	if (pipe(sigpipe) < 0) {
		return -1;
	}

	if (fd_nonblock(sigpipe[0]) < 0) {
		close(sigpipe[0]);
		close(sigpipe[1]);
		return -1;
	}

	memset(&sa, 0, sizeof(sa));
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sigall;

	for (i = 1; i < NSIG; i++) {
		sigaction(i, &sa, NULL);
	}

	loop_fd_add(sigpipe[0], LOOP_RD, sig_event, NULL);
	signals_notify = notify;

	return 0;
}

void sigs_reset(int signo)
{
	signals[signo] = 0;
}

void sigs_deinit(void)
{
	loop_fd_del(sigpipe[0]);
	close(sigpipe[0]);
	close(sigpipe[1]);
}

