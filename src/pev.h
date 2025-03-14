/* This is free and unencumbered software released into the public domain. */

/*
 * All pev callback run in regular process context, even signal
 * callbacks. All callbacks gets the socket, signal, or timer id,
 * as the first argument, in addition to the optional argument.
 *
 *     static void signal_cb(int signo, void *arg);
 *     static void socket_cb(int sd, void *arg);
 *     static void timer_cb(int id, void *arg);
 *
 * NOTE: pev < v2.0 passed the timeout value as the first argument
 *       to timer_cb().  Now the first arugment is the timer id.
 */

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
 * Destructor callback, called when deleting a signal event (pev_sig_del).
 * Useful for deallocating heap allocated arg data.
 */
int pev_sig_set_cb_del  (int id, void (*cb)(void *));

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
 * Destructor callback, called when deleting a socket event (pev_sock_del or
 * pev_sock_close). Useful for deallocating heap allocated arg data.
 */
int pev_sock_set_cb_del  (int id, void (*cb)(void *));


/*
 * TIMER API
 *
 * The scheduling granularity of timers is subject to limits in your
 * operating system timer resolution.
 *
 * Periodic timers use SIGALRM via the POSIX setitimer() API, which
 * may affect the use of sleep(), usleep(), and alarm() APIs in your
 * application.  See your respective OS for details.
 */

/*
 * Add or delete a timer.  When creating a new timer, the timeout and
 * period arguments are in microseconds.  The callback used to get the
 * timeout value as its first argument, but this was changed in v2.0 to
 * instead pass the timer id so callback easily can do pev_timer_set().
 * The timer id is also returned by the add function.
 *
 * For one-shot timers, set period = 0 and timeout to the delay before
 * the callback should be called.
 *
 * For periodic timers, set period != 0.  The timeout may be set to
 * zero (timeout=0) for periodic tasks, this means the first call will
 * be after period microseconds.  The timeout value is only used for
 * the first call.
 */
int pev_timer_add  (int timeout, int period, void (*cb)(int, void *), void *arg);
int pev_timer_del  (int id);

/*
 * Reset timeout of one-shot timer.  When a one-shot timer has fired
 * it goes inert.  Calling pev_timer_set() rearms the timer.
 *
 * Remember, the timeout argument is in microseconds.
 *
 * An active timer has a non-zero timeout, this can be checked with
 * the pev_timer_get() call.  This also applies to periodic timers,
 * with the exception of the first initial timeout, the call always
 * returns the period value.
 */
int pev_timer_set  (int id, int timeout);
int pev_timer_get  (int id);

/*
 * Destructor callback, called when deleting a timer (pev_timer_del).
 * Useful for deallocating heap allocated arg data.
 */
int pev_timer_set_cb_del  (int id, void (*cb)(void *));

#endif /* PEV_H_ */
