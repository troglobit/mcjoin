/*
 * Copyright (c) 2008-2020  Joachim Wiberg <troglobit()gmail!com>
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

#include "config.h"

#define SYSLOG_NAMES
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef HAVE_UTILITY_H
#include <utility.h>		/* MIN() SYSV */
#else
#include <sys/param.h>		/* MIN() */
#endif

#include "log.h"
#include "mcjoin.h"
#include "screen.h"

char **log_buf;			/* Ring buffer of LOG_MAX entries of width length */

#ifdef SYSV			/* Only SVR4, the BSD's and Linux have it */
#define INTERNAL_INVPRI 0x00	/* Value to indicate no priority in f_pmask */
#define INTERNAL_NOPRI  0x10	/* the "no priority" priority */
				/* mark "facility" */
#define INTERNAL_ALLPRI 0xFF   /* Value to indicate all priorities in f_pmask */
#define INTERNAL_MARK   LOG_MAKEPRI(LOG_NFACILITIES, 0)

typedef struct _code {
	char	*c_name;
	int	 c_val;
} CODE;

CODE prioritynames[] = {
	{ "alert",	LOG_ALERT },
	{ "crit",	LOG_CRIT },
	{ "debug",	LOG_DEBUG },
	{ "emerg",	LOG_EMERG },
	{ "err",	LOG_ERR },
	{ "error",	LOG_ERR },		/* DEPRECATED */
	{ "info",	LOG_INFO },
	{ "none",	INTERNAL_NOPRI },	/* INTERNAL */
	{ "notice",	LOG_NOTICE },
	{ "panic",	LOG_EMERG },		/* DEPRECATED */
	{ "warn",	LOG_WARNING },		/* DEPRECATED */
	{ "warning",	LOG_WARNING },
	{ "*",		INTERNAL_ALLPRI },	/* INTERNAL */
	{ NULL,		-1 }
};
#endif /* SYSV */

static int log_prio   = LOG_NOTICE;
static int log_syslog = 0;
static int log_ui     = 0;
static int log_max    = 0;
static int log_width  = 0;
static int log_opts   = LOG_NDELAY | LOG_PID;

int log_init(int fg, char *ident)
{
	if (fg) {
		int i;

		if (old)
			return 0;

		log_max = LOG_MAX;
		if (log_max < 100)
			log_max = 100;
		log_width = width;
		if (log_width < 256)
			log_width = 256;

		log_buf = calloc(log_max, sizeof(char *));
		if (!log_buf)
			return -1;

		for (i = 0; i < log_max; i++) {
			log_buf[i] = calloc(log_width, sizeof(char));
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

		for (i = 0; i < log_max; i++)
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

	if (!level)
		return log_prio;

	for (i = 0; prioritynames[i].c_name; i++) {
		size_t len = MIN(strlen(prioritynames[i].c_name), strlen(level));

		if (strncasecmp(prioritynames[i].c_name, level, len))
			continue;

		log_prio = prioritynames[i].c_val;
		return 0;
	}

	if (!isdigit(level[0])) {
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	i = strtol(level, NULL, 0);
	if (errno)
		return -1;

	log_prio = i;

	return 0;
}

void log_show(int signo)
{
	int lwh = height - LOG_ROW; /* log window height */
	int y = LOG_ROW;
	int i;

	(void)signo;

	gotoxy(0, LOG_ROW);
	clsdn();

	for (i = log_max - lwh; i < log_max; i++) {
		char line[width];

		if (y >= height)
			break;

		if (strlen(log_buf[i]) < 2)
			continue;

		gotoxy(0, y++);
		strlcpy(line, log_buf[i], sizeof(line));
		fputs(line, stderr);
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
			char buf[log_width];
			char *ptr;
			int i;

			for (i = 0; i < LOG_POS; i++)
				strlcpy(log_buf[i], log_buf[i + 1], log_width);

			now = time(NULL);
			snow = ctime(&now);

			vsnprintf(buf, sizeof(buf), fmt, ap);
			for (ptr = buf; *ptr && isspace((int)*ptr); ptr++)
				;

			snprintf(log_buf[LOG_POS], log_width, "%24.24s  %s", snow, ptr);
			log_show(0);
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
