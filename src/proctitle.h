#ifndef PROCTITLE_H
#define PROCTITLE_H

void proctitle_init(char *argv[], char *envp[]);

void proctitle_set(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));

void proctitle_vset(const char *fmt, va_list ap);

#endif
