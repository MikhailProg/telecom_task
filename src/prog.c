#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <err.h>

#include "loop.h"
#include "utils.h"
#include "proctitle.h"
#include "device.h"

static Device device;
static int sigpipe[2];
static int signals[NSIG];

static void env_opts_parse(int *host, int *is_controller)
{
	char *s = getenv("HOST_ADDR");
	if (s == NULL || (*host = atoi(s)) < 0 || *host > DEVICE_HOST_ADDR_MAX) {
		errx(EXIT_FAILURE, "provide HOST_ADDR variable [0, %d]",
							DEVICE_HOST_ADDR_MAX);
	}

	*is_controller = getenv("CONTROLLER") ? 1 : 0;
}

static void sig_event(int fd, LoopEvent event, void *opaque)
{
	char buf[32];

	(void)opaque;

	if (!(event & LOOP_RD))
		return;

	/* Drain all data from event fd till EAGAIN. */
	while (read(fd, buf, sizeof(buf)) > 0)
		;

	if (signals[SIGALRM]) {
		signals[SIGALRM] = 0;
		device.on_timeout(&device);
	}

	if (signals[SIGTERM] || signals[SIGINT]) {
		signals[SIGTERM] = signals[SIGINT] = 0;
		loop_quit();
	}
}

static void sigall(int signo)
{
	unsigned char a = 42;

	signals[signo] = 1;
	(void)!write(sigpipe[1], &a, 1);
}

static void siginit(void)
{
	struct sigaction sa;
	int i;
	int sigs[] = {
		SIGALRM, SIGTERM, SIGINT
	};

	if (pipe(sigpipe) < 0) {
		err(EXIT_FAILURE, "pipe()");
	}

	if (fd_nonblock(sigpipe[0]) < 0) {
		err(EXIT_FAILURE, "fd_nonblock()");
	}

	signal(SIGPIPE, SIG_IGN);

	memset(&sa, 0, sizeof(sa));
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sigall;

	for (i = 0; i < (int)ARRSZ(sigs); i++) {
		sigaction(sigs[i], &sa, NULL);
	}

	loop_fd_add(sigpipe[0], LOOP_RD, sig_event, NULL);
}

static void sigdeinit(void)
{
	loop_fd_del(sigpipe[0]);
	close(sigpipe[0]);
	close(sigpipe[1]);
}

static void resched_timer(struct device *dev, int secs)
{
	(void)dev;
	/* Forget current timeout, we are not interested in it anymore. */
	signals[SIGALRM] = 0;
	alarm(secs);
}

static void display(struct device *dev, const char *fmt, ...)
{
	va_list ap;

	(void)dev;
	va_start(ap, fmt);
	proctitle_vset(fmt, ap);
	va_end(ap);
}

int main(int argc, char *argv[], char *envp[])
{
	int host, is_controller;

	(void)argc;

	srand(time(NULL));
	proctitle_init(argv, envp);
	env_opts_parse(&host, &is_controller);

	if (loop_init(LOOP_DRV_DEFAULT) < 0) {
		errx(EXIT_FAILURE, "loop_init() failed");
	}

	siginit();
	if (device_init(&device, host, is_controller) < 0) {
		errx(EXIT_FAILURE, "device_init() failed");
	}

	device.resched_timer = resched_timer;
	device.display = display;
	device_start(&device);

	loop_run();

	device_deinit(&device);
	sigdeinit();
	loop_fini();

	return EXIT_SUCCESS;
}
