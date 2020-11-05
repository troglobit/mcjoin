/*
 * Copyright (c) 2018-2020  Joachim Wiberg <troglobit@gmail.com>
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

#define SYSLOG_NAMES
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>		/* MIN() */

#include "log.h"
#include "mcjoin.h"

static int log_prio   = LOG_NOTICE;
static int log_syslog = 0;
static int log_opts   = LOG_NDELAY | LOG_PID;

int log_init(int fg, char *ident)
{
	if (fg)
		return 0;

	log_syslog = 1;

	openlog(ident, log_opts, LOG_DAEMON);
	setlogmask(LOG_UPTO(log_prio));

	return 0;
}

int log_exit(void)
{
	if (!foreground)
		return 0;

	closelog();

	return 0;
}

int log_level(const char *level)
{
	int i;

	for (i = 0; prioritynames[i].c_name; i++) {
		size_t len = MIN(strlen(prioritynames[i].c_name), strlen(level));

		if (strncasecmp(prioritynames[i].c_name, level, len))
			continue;

		log_prio = prioritynames[i].c_val;
		return 0;
	}

	errno = 0;
	i = strtol(level, NULL, 0);
	if (errno)
		return -1;

	log_prio = i;

	return 0;
}

int logit(int prio, char *fmt, ...)
{
	va_list ap;
	int rc = 0;

	va_start(ap, fmt);
	if (log_syslog)
		vsyslog(prio, fmt, ap);
	else if (prio <= log_prio) {
		FILE *fp = stdout;
		int sync = 1;

		if (prio <= LOG_ERR) {
			fp = stderr;
			sync = 1;
		}

		rc = vfprintf(fp, fmt, ap);
		if (sync)
			fflush(fp);
	}
	va_end(ap);

	return rc;
}
