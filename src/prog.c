#include <sys/time.h>

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

static void env_opts_parse(int *host, int *iscontroller)
{
	char *s = getenv("HOST_ADDR");
	if (s == NULL || (*host = atoi(s)) < 0 || *host > DEVICE_HOST_ADDR_MAX) {
		errx(EXIT_FAILURE, "provide HOST_ADDR variable [0, %d]",
							DEVICE_HOST_ADDR_MAX);
	}

	*iscontroller = getenv("CONTROLLER") ? 1 : 0;
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

static void timer(const Device *dev, int msec)
{
	UNUSED(dev);
	/* Forget current timeout, we are not interested in it anymore. */
	sigs_reset(SIGALRM);
	struct itimerval tval = {
		{ 0 , 0 }, { msec / 1000, msec % 1000 * 1000 }
	};
	setitimer(ITIMER_REAL, &tval, NULL);
}

static void display(const Device *dev, const char *fmt, ...)
{
	va_list ap;

	UNUSED(dev);
	va_start(ap, fmt);
	proctitle_vset(fmt, ap);
	va_end(ap);
}


static void prog_init(int argc, char **argv, char **envp)
{
	UNUSED(argc);

	proctitle_init(argv, envp);
	srand(time(NULL));

	if (loop_init(LOOP_DRV_DEFAULT) < 0) {
		errx(EXIT_FAILURE, "loop_init() failed");
	}

	if (sigs_init(signals_notify) < 0) {
		errx(EXIT_FAILURE, "sigs_init() failed");
	}

	signal(SIGPIPE, SIG_IGN);
}


static void prog_deinit()
{
	sigs_deinit();
	loop_fini();
}

int main(int argc, char *argv[], char *envp[])
{
	int host, iscontroller;

	env_opts_parse(&host, &iscontroller);

	prog_init(argc, argv, envp);

	const DeviceOps ops = {
		display, timer
	};

	if (device_init(&device, host, iscontroller, &ops) < 0) {
		errx(EXIT_FAILURE, "device_init() failed");
	}

	device_run(&device);

	device_deinit(&device);

	prog_deinit();

	return EXIT_SUCCESS;
}
