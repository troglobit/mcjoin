/* This is free and unencumbered software released into the public domain. */

#ifndef PEV_H_
#define PEV_H_

#include <stdarg.h>
#include <string.h>
#include <sys/time.h>

/*
 * Call this before creating any signal, socket, or timer callbacks.
 */
int pev_init       (void);

/*
 * Call this from a callback to exit the event loop.
 * The status is what is returned from pev_run().
 */
int pev_exit       (int status);

/*
 * The event loop itself.  Call after pev_init() and all the signal,
 * socket, or timer callbacks have been created.  Returns the status
 * given to pev_exit().
 */
int pev_run        (void);

/*
 * Signal callbacks are identified by signal number, only one callback
 * per signal.  Signals are serialized like timers, which use SIGALRM,
 * using a pipe.  Delete by giving id returned from pev_sig_add()
 */
int pev_sig_add    (int signo, void (*cb)(int, void *), void *arg);
int pev_sig_del    (int id);

/*
 * Socket or file descriptor callback by sd/fd, only one callback per
 * descriptor.  API changes to CLOEXEC and NONBLOCK.  Delete by id
 * returned from pev_sock_add()
 */
int pev_sock_add   (int sd, void (*cb)(int, void *), void *arg);
int pev_sock_del   (int id);

/*
 * Same as pev_sock_add() but creates/closes socket as well.
 * Delete by id returned from pev_sock_open()
 */
int pev_sock_open  (int domain, int type, int proto, void (*cb)(int, void *), void *arg);
int pev_sock_close (int id);

/*
 * Periodic timers use SIGALRM via setitimer() API, may affect use of
 * sleep(), usleep(), and alarm() APIs.  See your respective OS for
 * details.  Otherwise works like the other pev APIs, returns id.
 * The period argument is in microseconds.  Scheduling granularity
 * is subject to limits in your operating system timer resolution.
 */
int pev_timer_add  (int period, void (*cb)(int, void *), void *arg);
int pev_timer_del  (int id);

#endif /* PEV_H_ */
