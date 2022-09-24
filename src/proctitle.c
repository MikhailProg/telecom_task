#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "proctitle.h"

static char *proctitle;
static int nproctitle;

void proctitle_init(char *argv[], char *envp[])
{
	char *last;
	size_t len = strlen(argv[0]);
	int i;

	last = argv[0] + len + 1;
	for (i = 1; argv[i] != NULL; i++) {
		if (argv[i] == last)
			last = argv[i] + strlen(argv[i]) + 1;
	}
	if (envp != NULL) {
		for (i = 0; envp[i] != NULL; i++) {
			if (envp[i] == last)
				last = envp[i] + strlen(envp[i]) + 1;
		}
	}

	proctitle = argv[0] + len;
	nproctitle = last - argv[0] - len;
}

void proctitle_set(const char *fmt, ...)
{
	va_list ap;
	memset(proctitle, 0, nproctitle);
	va_start(ap, fmt);
	vsnprintf(proctitle, nproctitle, fmt, ap);
	va_end(ap);
}

void proctitle_vset(const char *fmt, va_list ap)
{
	vsnprintf(proctitle, nproctitle, fmt, ap);
}

