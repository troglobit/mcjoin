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
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/param.h>		/* MIN() */

#include "log.h"
#include "mcjoin.h"
#include "screen.h"

char **log_buf;			/* Ring buffer of LOG_MAX entries of width length */

static int log_prio   = LOG_NOTICE;
static int log_syslog = 0;
static int log_ui     = 0;
static int log_opts   = LOG_NDELAY | LOG_PID;

int log_init(int fg, char *ident)
{
	if (fg) {
		int i;

		if (old)
			return 0;

		log_buf = calloc(LOG_MAX, sizeof(char *));
		if (!log_buf)
			return -1;

		for (i = 0; i < LOG_MAX; i++) {
			log_buf[i] = calloc(width, sizeof(char));
			if (!log_buf[i]) {
				while (i)
					free(log_buf[--i]);
				free(log_buf);
				return -1;
			}
		}

		log_ui = 1;
		return 0;
	}

	log_syslog = 1;

	openlog(ident, log_opts, LOG_DAEMON);
	setlogmask(LOG_UPTO(log_prio));

	return 0;
}

int log_exit(void)
{
	if (!foreground) {
		int i;

		for (i = 0; i < LOG_MAX; i++)
			free(log_buf[i]);
		if (log_buf)
			free(log_buf);

		return 0;
	}

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

static void display(void)
{
	int y = LOG_ROW;
	int i;

	gotoxy(0, LOG_ROW);
	clsdn();

	for (i = 0; i < LOG_MAX; i++) {
		if (y >= height)
			break;

		if (strlen(log_buf[i]) < 2)
			continue;

		gotoxy(0, y++);
		fputs(log_buf[i], stderr);
	}
}

int logit(int prio, char *fmt, ...)
{
	va_list ap;
	time_t now;
	char *snow;
	int rc = 0;

	va_start(ap, fmt);
	if (log_syslog)
		vsyslog(prio, fmt, ap);
	else if (prio <= log_prio) {
		if (log_ui) {
			char buf[width];
			char *ptr;
			int i;

			if (strlen(fmt) < 1)
				return -1; /* Too short for this mode, skip */

			for (i = LOG_MAX - 1; i > 0; i--)
				strcpy(log_buf[i], log_buf[i - 1]);

			now = time(NULL);
			snow = ctime(&now);

			vsnprintf(buf, sizeof(buf), fmt, ap);
			for (ptr = buf; *ptr && isspace(*ptr); ptr++)
				;

			snprintf(log_buf[LOG_POS], width, "%24.24s  %s", snow, ptr);
			display();
		} else {
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
	}
	va_end(ap);

	return rc;
}
