/* This is free and unencumbered software released into the public domain. */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "pev.h"

#define PEV_SOCK   1
#define PEV_TIMER  2
#define PEV_SIG    3

struct pev {
	struct pev *prev, *next;

	int id;
	char type;
	signed char active;

	union {
		int sd;
		int signo;
		struct {
			int timeout;
			int period;
			struct timespec expiry;
		};
	};

	void (*cb)(int, void *);
	void *arg;
};

struct pev *pl;

static int events[2];
static int max_fdnum = -1;
static int id = 1;
static int running;
static int status;

static struct pev *pev_new  (int type, void (*cb)(int, void *), void *arg);
static struct pev *pev_find (int type, int signo);

/******************************* SIGNALS ******************************/

static void sig_handler(int signo)
{
	char buf[1] = { (char)signo };

	while (write(events[1], buf, 1) < 0) {
		if (errno != EINTR)
			return;
	}
}

int pev_sig_add(int signo, void (*cb)(int, void *), void *arg)
{
	struct sigaction sa = { 0 };
	struct pev *entry;

	if (pev_find(PEV_SIG, signo)) {
		errno = EEXIST;
		return -1;
	}

	entry = pev_new(PEV_SIG, cb, arg);
	if (!entry)
		return -1;

	sa.sa_handler = sig_handler;
	sa.sa_flags = SA_RESTART;
	sigaction(signo, &sa, NULL);

	entry->signo = signo;

	return entry->id;
}

int pev_sig_del(int id)
{
	struct pev *entry;

	entry = pev_find(PEV_SIG, id);
	if (!entry)
		return -1;

	sigaction(entry->signo, NULL, NULL);

	return pev_sock_del(id);
}

/******************************* SOCKETS ******************************/

static int nfds(void)
{
	return max_fdnum + 1;
}

static void sock_run(fd_set *fds)
{
	struct pev *entry;

	FD_ZERO(fds);
	for (entry = pl; entry; entry = entry->next) {
		if (entry->type != PEV_SOCK)
			continue;

		FD_SET(entry->sd, fds);
	}
}

int pev_sock_add(int sd, void (*cb)(int, void *), void *arg)
{
	struct pev *entry;
	int rc;

	if (sd < 0) {
		errno = EINVAL;
		return -1;
	}

	rc = fcntl(sd, F_GETFD);
	if (rc == -1)
		return -1;
	rc = fcntl(sd, F_SETFD, rc | O_CLOEXEC | O_NONBLOCK);
	if (rc == -1)
		return -1;

	entry = pev_new(PEV_SOCK, cb, arg);
	if (!entry)
		return -1;

	entry->sd = sd;

	/* Keep track for select() */
	if (sd > max_fdnum)
		max_fdnum = sd;

	return entry->id;
}

int pev_sock_del(int id)
{
	struct pev *entry;

	for (entry = pl; entry; entry = entry->next) {
		if (entry->id == id) {
			/* Mark for deletion and issue a new run */
			entry->active = 0;
			sig_handler(0);

			return 0;
		}
	}

	errno = ENOENT;
	return -1;
}

int pev_sock_open(int domain, int type, int proto, void (*cb)(int, void *), void *arg)
{
	int sd;

	if (!cb) {
		errno = EINVAL;
		return -1;
	}

	sd = socket(domain, type, proto);
	if (sd < 0)
		return -1;

	if (pev_sock_add(sd, cb, arg) < 0) {
		close(sd);
		return -1;
	}

	return sd;
}

int pev_sock_close(int sd)
{
	struct pev *entry;

	entry = pev_find(PEV_SOCK, sd);
	if (!entry)
		return -1;

	pev_sock_del(entry->id);
	close(sd);

	return 0;
}

/******************************* TIMERS *******************************/

static struct pev *timer_ffs(void)
{
	struct pev *entry;

	for (entry = pl; entry; entry = entry->next) {
		if (entry->type == PEV_TIMER && entry->active > 0)
			return entry;
	}

	return NULL;
}

static struct pev *timer_compare(struct pev *a, struct pev *b)
{
	if (b->type != PEV_TIMER || b->active < 1)
		return a;

	if (a->expiry.tv_sec < b->expiry.tv_sec)
		return a;

	if (a->expiry.tv_sec == b->expiry.tv_sec &&
	    a->expiry.tv_nsec <= b->expiry.tv_nsec)
		return a;

	return b;
}

static int timer_start(struct timespec *now)
{
	struct itimerval it = { 0 };
	struct pev *next, *entry;

	next = timer_ffs();
	if (!next)
		return -1;

	for (entry = pl; entry; entry = entry->next)
		next = timer_compare(next, entry);

	it.it_value.tv_sec  =  next->expiry.tv_sec  - now->tv_sec;
	it.it_value.tv_usec = (next->expiry.tv_nsec - now->tv_nsec) / 1000;
	if (it.it_value.tv_usec < 0) {
		it.it_value.tv_sec -= 1;
		it.it_value.tv_usec = 1000000 + it.it_value.tv_usec;
	}

	/* Sanity check resulting value, prevent disabling timer */
	if (it.it_value.tv_sec < 0)
		it.it_value.tv_sec = 0;
	if (it.it_value.tv_sec == 0 && it.it_value.tv_usec < 1)
		it.it_value.tv_usec = 1;

	return setitimer(ITIMER_REAL, &it, NULL);
}

static int timer_expired(struct pev *entry, struct timespec *now)
{
	if (entry->type != PEV_TIMER || entry->active < 1)
		return 0;

	if (entry->expiry.tv_sec < now->tv_sec)
		return 1;

	if (entry->expiry.tv_sec == now->tv_sec &&
	    entry->expiry.tv_nsec <= now->tv_nsec)
		return 1;

	return 0;
}

static void timer_run(int signo, void *arg)
{
	struct pev *entry, *next;
	struct timespec now;

	(void)arg;
	clock_gettime(CLOCK_MONOTONIC, &now);

	for (entry = pl; entry; entry = next) {
		unsigned int sec, usec;
		int timeout;

		next = entry->next;
		if (entry->type != PEV_TIMER)
			continue;

		if (!timer_expired(entry, &now))
			continue;

		if (entry->timeout)
			timeout = entry->timeout;
		else
			timeout = entry->period;

		if (signo && entry->cb) {
			entry->timeout = 0;

			entry->cb(timeout, entry->arg);
			if (!entry->period && !entry->timeout) {
				entry->active = -1;
				continue;
			}
		}

		sec  = timeout / 1000000;
		usec = timeout % 1000000;
		entry->expiry.tv_sec  = now.tv_sec + sec;
		entry->expiry.tv_nsec = now.tv_nsec + (usec * 1000);
		if (entry->expiry.tv_nsec > 1000000000) {
			entry->expiry.tv_sec++;
			entry->expiry.tv_nsec -= 1000000000;
		}
	}

	timer_start(&now);
}

static int timer_init(void)
{
	return pev_sig_add(SIGALRM, timer_run, NULL);
}

static int timer_exit(void)
{
	struct itimerval it = { 0 };

	return setitimer(ITIMER_REAL, &it, NULL);
}

int pev_timer_add(int timeout, int period, void (*cb)(int, void *), void *arg)
{
	struct pev *entry;

	if (timeout <= 0 && period <= 0) {
		errno = EINVAL;
		return -1;
	}

	entry = pev_new(PEV_TIMER, cb, arg);
	if (!entry)
		return -1;

	entry->timeout = timeout;
	entry->period  = period;
	entry->active++;

	return entry->id;
}

int pev_timer_del(int id)
{
	return pev_sock_del(id);
}

int pev_timer_set(int id, int timeout)
{
	struct pev *entry;

	for (entry = pl; entry; entry = entry->next) {
		if (entry->type != PEV_TIMER)
			continue;
		if (entry->id != id)
			continue;

		entry->timeout = timeout;
		memset(&entry->expiry, 0, sizeof(entry->expiry));
		entry->active = 2;
		return 0;
	}

	errno = ENOENT;
	return -1;
}

int pev_timer_get(int id)
{
	struct pev *entry;

	for (entry = pl; entry; entry = entry->next) {
		if (entry->type != PEV_TIMER)
			continue;
		if (entry->id != id)
			continue;

		if (entry->timeout)
			return entry->timeout;

		return entry->period;
	}

	errno = ENOENT;
	return -1;
}

/******************************* GENERIC ******************************/

static struct pev *pev_new(int type, void (*cb)(int, void *), void *arg)
{
	struct pev *entry;

	if (!cb) {
		errno = EINVAL;
		return NULL;
	}

	entry = calloc(1, sizeof(*entry));
	if (!entry)
		return NULL;

	entry->id = id++;
	entry->type = type;
	entry->active = 1;

	entry->cb  = cb;
	entry->arg = arg;

	entry->next = pl;
	entry->prev = NULL;
	if (entry->next)
		entry->next->prev = entry;
	pl = entry;

	return entry;
}

static struct pev *pev_find(int type, int signo)
{
	struct pev *entry;

	for (entry = pl; entry; entry = entry->next) {
		if (entry->type != type)
			continue;

		if (entry->signo != signo)
			continue;

		return entry;
	}

	errno = ENOENT;
	return NULL;
}

static void pev_cleanup(void)
{
	struct pev *entry, *next, *prev;

	for (entry = pl; entry; entry = next) {
		next = entry->next;
		prev = entry->prev;

		if (entry->active)
			continue;

		if (next)
			next->prev = prev;
		if (prev)
			prev->next = next;
		else
			pl = next;

		free(entry);
	}
}

static void pev_event(int sd, void *arg)
{
	struct pev *entry;
	char signo;

	(void)arg;
	while (read(sd, &signo, 1) < 0) {
		if (errno != EINTR)
			return;
	}

	entry = pev_find(PEV_SIG, signo);
	if (!entry)
		return;

	if (!entry->cb)
		return;

	entry->cb(entry->signo, entry->arg);
}

int pev_init(void)
{
	if (pipe(events))
		return -1;
	if (pev_sock_add(events[0], pev_event, NULL) < 0)
		return -1;

	running = 1;

	return timer_init();
}

int pev_exit(int rc)
{
	struct pev *entry;

	pev_sock_close(events[0]);
	pev_sock_close(events[1]);

	for (entry = pl; entry; entry = entry->next)
		entry->active = 0;

	running = 0;
	status = rc;

	return timer_exit();
}

static void pev_check(fd_set *fds)
{
	struct pev *entry;
	int trestart = 0;

	sock_run(fds);
	pev_cleanup();

	for (entry = pl; entry; entry = entry->next) {
		if (entry->type == PEV_TIMER && entry->active > 1) {
			entry->active = 1;
			trestart = 1;
		}
	}

	if (trestart)
		timer_run(0, NULL);
}

int pev_run(void)
{
	struct pev *entry, *next;
	fd_set fds;
	int num;

	while (running) {
		pev_check(&fds);

		errno = 0;
		num = select(nfds(), &fds, NULL, NULL, NULL);
		if (num <= 0)
			continue;

		for (entry = pl; entry; entry = next) {
			next = entry->next;

			if (entry->type != PEV_SOCK)
				continue;

			if (!FD_ISSET(entry->sd, &fds))
				continue;

			if (entry->cb)
				entry->cb(entry->sd, entry->arg);
		}
	}
	pev_cleanup();

	return status;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
