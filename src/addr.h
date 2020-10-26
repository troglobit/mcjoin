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

#ifndef MCJOIN_ADDR_H_
#define MCJOIN_ADDR_H_

#define __APPLE_USE_RFC_3542 1		/* On Apple macOS */

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifndef IN_LINKLOCAL
#define IN_LINKLOCALNETNUM 0xa9fe0000
#define IN_LINKLOCAL(addr) ((addr & IN_CLASSB_NET) == IN_LINKLOCALNETNUM)
#endif

#ifndef IN_LOOPBACK
#define IN_LOOPBACK(addr) ((addr & IN_CLASSA_NET) == 0x7f000000)
#endif

#ifndef IN_ZERONET
#define IN_ZERONET(addr) ((addr & IN_CLASSA_NET) == 0)
#endif

/* From The Practice of Programming, by Kernighan and Pike */
#ifndef NELEMS
#define NELEMS(array) (sizeof(array) / sizeof(array[0]))
#endif

#ifdef AF_INET6
#define INET_ADDRSTR_LEN  INET6_ADDRSTRLEN
#else
#define INET_ADDRSTR_LEN  INET_ADDRSTRLEN
#endif
typedef struct sockaddr_storage inet_addr_t;

/* Group info */
struct gr {
	int          sd;
	size_t       count;
	size_t       seq;
	size_t       gaps;
	char        *source;
	char        *group;
	inet_addr_t  src;
	inet_addr_t  grp;	/* to */
};

const char *inet_address(inet_addr_t *ss, char *buf, size_t len);
socklen_t   inet_addrlen(inet_addr_t *ss);

void  inet_addr_set_port(inet_addr_t *ss, short port);
short inet_addr_get_port(inet_addr_t *ss);

char *ifdefault (char *iface, size_t len);
int   ifinfo    (char *iface, inet_addr_t *addr, int family);

#endif /* MCJOIN_ADDR_H_ */
