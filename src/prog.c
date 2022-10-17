#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <err.h>

#include "loop.h"
#include "utils.h"
#include "proctitle.h"
#include "device.h"
#include "sigs.h"

static Device device;

static void env_opts_parse(int *host, int *is_controller)
{
	char *s = getenv("HOST_ADDR");
	if (s == NULL || (*host = atoi(s)) < 0 || *host > DEVICE_HOST_ADDR_MAX) {
		errx(EXIT_FAILURE, "provide HOST_ADDR variable [0, %d]",
							DEVICE_HOST_ADDR_MAX);
	}

	*is_controller = getenv("CONTROLLER") ? 1 : 0;
}

static void signals_notify(sigset_t *sigmask)
{
	if (sigismember(sigmask, SIGALRM)) {
		device_timeout(&device);
	}

	if (sigismember(sigmask, SIGTERM) || sigismember(sigmask, SIGINT)) {
		loop_quit();
	}
}

static void resched_timer(const Device *dev, int secs)
{
	UNUSED(dev);
	/* Forget current timeout, we are not interested in it anymore. */
	sigs_reset(SIGALRM);
	alarm(secs);
}

static void display(const Device *dev, const char *fmt, ...)
{
	va_list ap;

	UNUSED(dev);
	va_start(ap, fmt);
	proctitle_vset(fmt, ap);
	va_end(ap);
}

int main(int argc, char *argv[], char *envp[])
{
	int host, is_controller;

	UNUSED(argc);

	srand(time(NULL));
	proctitle_init(argv, envp);
	env_opts_parse(&host, &is_controller);

	if (loop_init(LOOP_DRV_DEFAULT) < 0) {
		errx(EXIT_FAILURE, "loop_init() failed");
	}

	if (sigs_init(signals_notify) < 0) {
		errx(EXIT_FAILURE, "sigs_init() failed");
	}

	signal(SIGPIPE, SIG_IGN);

	const DeviceOps ops = {
		display, resched_timer
	};

	if (device_init(&device, host, is_controller, &ops) < 0) {
		errx(EXIT_FAILURE, "device_init() failed");
	}

	device_start(&device);

	loop_run();

	device_deinit(&device);
	sigs_deinit();
	loop_fini();

	return EXIT_SUCCESS;
}
