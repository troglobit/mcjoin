/*
 * Copyright (c) 2008-2021  Joachim Wiberg <troglobit()gmail!com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef MCJOIN_H_
#define MCJOIN_H_

#include <sys/queue.h>

#include "config.h"
#include "addr.h"
#include "log.h"
#include "pev.h"

#define BUFSZ           1606	/* +42 => 1648 */
#define MAX_NUM_GROUPS  2048
#define DEFAULT_GROUP   "225.1.2.3"
#define DEFAULT_PORT    1234
#define MAGIC_KEY       "Sender PID "
#define SEQ_KEY         "count: "
#define FREQ_KEY        "freq: "

#define STATUS_HISTORY  1024
#define STATUS_POS      (STATUS_HISTORY - 2)

/* Positions on screen for ui */
#define TITLE_ROW       1
#define HOSTDATE_ROW    2
#define HEADING_ROW     3
#define GROUP_ROW       4
#define LOGHEADING_ROW  (group_num + GROUP_ROW + 1)
#define LOG_ROW         (LOGHEADING_ROW + 1)
#define EXIT_ROW        (LOG_ROW + LOG_MAX)

/* From The Practice of Programming, by Kernighan and Pike */
#ifndef NELEMS
#define NELEMS(array) (sizeof(array) / sizeof(array[0]))
#endif

/* Group info */
struct gr {
	int          sd;
	uint64_t     bytes;
	uint64_t     obytes;
	size_t       rate;
	size_t       count;
	size_t       seq;
	size_t       gaps;
	size_t       dupes;
	size_t       order;
	size_t       delayed;
	size_t       invalid;
	char        *source;
	char        *group;
	inet_addr_t  src;
	inet_addr_t  grp;	/* to */
	size_t       seqnos[STATUS_HISTORY]; /* for dup detection */
	char         status[STATUS_HISTORY];
	size_t       spin;
	TAILQ_ENTRY(gr) entry;
};

extern int help;
extern int pres;
extern int width;
extern int height;
extern int duplicate;
extern int foreground;

extern int need4;
extern int need6;

extern char iface[];

extern int period;
extern size_t bytes;
extern size_t count;
extern unsigned char ttl;

extern size_t group_num;

extern void plotter_show(int signo);

/* strlcpy.c */
#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t len);
#endif

/* daemonize.c */
extern int daemonize     (void);

/* receiver.c */
extern int receiver_init (void);
extern int receiver      (int count);

/* sender.c */
extern int sender_init   (void);

#endif /* MCJOIN_H_ */
