/*
 * Copyright (c) 2008-2025  Joachim Wiberg <troglobit()gmail!com>
 * Copyright (C) 2021       Adam Young <adamyg()gmail!com>
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

#ifndef MCJOIN_INETADDR_H_
#define MCJOIN_INETADDR_H_

int inet_ip6(const char *addr);
int inet_port(const char *value);
int inet_pton_port(int family, const char *addr, void *ptr, int *sin_port, int def_port);

#endif /* MCJOIN_INETADDR_H_ */
