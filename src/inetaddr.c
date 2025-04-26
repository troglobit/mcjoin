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

#include <arpa/inet.h>		/* inet_pton() on Solaris */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(INETADDR_MAIN)
#define ERROR(fmt, args...) do { printf(fmt "\n", ##args); } while (0)
#else
#include "log.h"
#endif

#ifdef AF_INET6
/* two or more colons, assume ip6 */
int inet_ip6(const char *ip)
{
	const char *cursor = ip;
	unsigned colons = 0;

	while ((cursor = strchr(cursor, ':'))) {
		cursor++;
		colons++;
	}

	return '[' == *ip || colons > 1;
}
#endif

int inet_port(const char *value)
{
	char *ep = (char *)value;
	int port;

	if (!value)
		return -1;

	port = strtoll(value, &ep, 10);
	if (value == ep || *ep || port < 0 || port > 0xffff)
		return -1;

	return port;
}

int inet_pton_port(int family, const char *addr, void *ptr, int *sin_port, int def_port)
{
	int port = def_port, rc = -1;
	char *t_addr;

	if (!addr) {
		errno = EINVAL;
		return -1;
	}

	t_addr = strdup(addr);
	if (!t_addr)
		return -1;

	addr = t_addr;
#ifdef AF_INET6
	if (family == AF_INET6) {
		/* [1fff:0:a88:85a3::ac1f]:8001 or 1fff:0:a88:85a3::ac1f */
		if ('[' == *addr) {
			/* optional [] enclosure. */
			char *end_spec = strchr(++addr, ']');

			if (!end_spec)
				goto error;

			*end_spec++ = 0;
			if (*end_spec) {
				/* optional trailing port */
				if (':' != *end_spec++)
					goto error;

				port = inet_port(end_spec);
				if (port < 0)
					goto error;
			}
		}
	} else
#endif
	{
		/* 1.2.3.4:8001 or 1.2.3.4 */
		char *port_spec = strchr(addr, ':');

		if (port_spec) {
			/* optional trailing port */
			*port_spec++ = 0;

			port = inet_port(port_spec);
			if (port < 0)
				goto error;
		}
	}

	rc = inet_pton(family, addr, ptr);
	if (rc == -1 || rc == 0) {
		rc = -1;
		goto error;
	}

	rc = 0;
	if (sin_port)
		*sin_port = port;

error:
	free(t_addr);
	return rc;
}


#if defined(INETADDR_MAIN)

#define TEST(__x)	test(__x, #__x)

static void test(int val, const char *msg)
{
	printf("[%s] %s\n", (val ? " OK  " : "ERROR"), msg);
}

int main(void)
{
	union {
		struct sockaddr_in6 ip6;
		struct sockaddr_in ip4;
	} addr;
	int port;

	TEST((port = inet_port(NULL)) < 0);
	TEST((port = inet_port("")) < 0);
	TEST((port = inet_port("-1")) < 0);
	TEST((port = inet_port("1024x")) < 0);
	TEST((port = inet_port("1024 ")) < 0);
	TEST((port = inet_port("65537")) < 0);

	TEST((port = inet_port("1024")) == 1024);
	TEST((port = inet_port("65535")) == 65535);

	TEST(-1 == inet_pton_port(AF_INET, NULL, &addr, &port, -1));
	TEST(-1 == inet_pton_port(AF_INET, "", &addr, &port, -1));

	TEST(-1 == inet_pton_port(AF_INET, "1.2.3.4:", &addr, &port, -1));
	TEST(-1 == inet_pton_port(AF_INET, "1.2.3.4: ", &addr, &port, -1));
	TEST(-1 == inet_pton_port(AF_INET, "1.2.3.4:-1", &addr, &port, -1));
	TEST(-1 == inet_pton_port(AF_INET, "1.2.3.4:99999", &addr, &port, -1));
	TEST(-1 == inet_pton_port(AF_INET, "1.2.3.4:1024x", &addr, &port, -1));

	TEST( 0 == inet_pton_port(AF_INET, "1.2.3.4", &addr, &port, -1) && port == -1);
	TEST( 0 == inet_pton_port(AF_INET, "1.2.3.4:1024", &addr, &port, -1) && port == 1024);

	TEST(-1 == inet_pton_port(AF_INET6, "0:0:0:0:0:0:0:0]:1024", &addr, &port, -1));
	TEST(-1 == inet_pton_port(AF_INET6, "[0:0:0:0:0:0:0:0:1024", &addr, &port, -1));
	TEST(-1 == inet_pton_port(AF_INET6, "[0:0:0:0:0:0:0:0] ", &addr, &port, -1));
	TEST(-1 == inet_pton_port(AF_INET6, "[0:0:0:0:0:0:0:0]:", &addr, &port, -1));
	TEST(-1 == inet_pton_port(AF_INET6, "[0:0:0:0:0:0:0:0]: ", &addr, &port, -1));
	TEST(-1 == inet_pton_port(AF_INET6, "[0:0:0:0:0:0:0:0]:1024x", &addr, &port, -1));

	TEST(0 == inet_pton_port(AF_INET6, "0:0:0:0:0:0:0:0", &addr, &port, -1) && port == -1);
	TEST(0 == inet_pton_port(AF_INET6, "[0:0:0:0:0:0:0:0]", &addr, &port, -1) && port == -1);
	TEST(0 == inet_pton_port(AF_INET6, "::", &addr, &port, -1) && port == -1);
	TEST(0 == inet_pton_port(AF_INET6, "[::]", &addr, &port, -1) && port == -1);

	TEST(0 == inet_pton_port(AF_INET6, "1:0:0:0:0:0:0:8", &addr, &port, -1) && port == -1);
	TEST(0 == inet_pton_port(AF_INET6, "1::8", &addr, &port, -1) && port == -1);
	TEST(0 == inet_pton_port(AF_INET6, "0:0:0:0:0:FFFF:204.152.189.116", &addr, &port, -1) && port == -1);
	TEST(0 == inet_pton_port(AF_INET6, "::ffff:204.152.189.116", &addr, &port, -1) && port == -1);

	TEST(0 == inet_pton_port(AF_INET6, "[0:0:0:0:0:0:0:0]:1024", &addr, &port, -1) && port == 1024);
	TEST(0 == inet_pton_port(AF_INET6, "[::]:1025", &addr, &port, -1) && port == 1025);
	TEST(0 == inet_pton_port(AF_INET6, "[1:0:0:0:0:0:0:8]:1026", &addr, &port, -1) && port == 1026);
	TEST(0 == inet_pton_port(AF_INET6, "[1::8]:1027", &addr, &port, -1) && port == 1027);
	TEST(0 == inet_pton_port(AF_INET6, "[0:0:0:0:0:FFFF:204.152.189.116]:1028", &addr, &port, -1) && port == 1028);
	TEST(0 == inet_pton_port(AF_INET6, "[::ffff:204.152.189.116]:1029", &addr, &port, -1) && port == 1029);

	return 0;
}

#endif	/*INETADDR_MAIN*/
