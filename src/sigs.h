#ifndef SIGS_H
#define SIGS_H

int sigs_init(void (*notify)(sigset_t *sigmask));

void sigs_reset(int signo);

void sigs_deinit(void);

#endif /* SIGS_H */
