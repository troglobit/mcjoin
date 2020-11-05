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

#ifndef MCJOIN_LOG_H_
#define MCJOIN_LOG_H_

#include <syslog.h>

#define LOG_MAX  3

#define DEBUG(fmt, args...) do { logit(LOG_DEBUG,  fmt "\n", ##args); } while (0)
#define ERROR(fmt, args...) do { logit(LOG_ERR,    fmt "\n", ##args); } while (0)
#define PRINT(fmt, args...) do { logit(LOG_NOTICE, fmt "\n", ##args); } while (0)

extern int log_level;
extern int log_syslog;
extern int log_opts;

int logon(void);
int loglvl(const char *level);
int logit(int prio, char *fmt, ...);

#endif /* MCJOIN_LOG_H_ */
