/* Send multicast data to group(s) and display progress
 *
 * Copyright (C) 2004       David Stevens <dlstevens()us!ibm!com>
 * Copyright (C) 2008-2020  Joachim Wiberg <troglobit()gmail!com>
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
#include "mcjoin.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


static int send_socket(int family)
{
	inet_addr_t addr;
	char buf[INET_ADDRSTR_LEN];
	int ifindex;
	int sd;

	ifindex = ifinfo(iface, &addr, family);
	if (ifindex <= 0) {
		if (!iface[0])
			ERROR("No outbound interface available, use `-i IFNAME`.");
		else {
			char msg[80];

			snprintf(msg, sizeof(msg), "Interface %s has no IPv%s address yet",
				 iface, family == AF_INET ? "4" : "6");
			if (log_level(NULL) == LOG_DEBUG && errno != EINTR)
				ERROR("%s, rc %d: %s", msg, ifindex, strerror(errno));
			else
				ERROR("%s.", msg);
		}
		return -1;
	}

	sd = socket(family, SOCK_DGRAM, 0);
	if (sd < 0) {
		ERROR("Failed opening socket(): %s", strerror(errno));
		return -1;
	}

	PRINT("Sending IPv%s multicast on %s addr, %s ifindex: %d, sd: %d",
	      family == AF_INET ? "4" : "6", iface,
	      inet_address(&addr, buf, sizeof(buf)), ifindex, sd);

	if (family == AF_INET) {
#ifdef HAVE_STRUCT_IP_MREQN_IMR_IFINDEX
		struct ip_mreqn imr = { .imr_ifindex = ifindex };
#else
		struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
		struct in_addr ina = sin->sin_addr;
#endif

		if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)))
			ERROR("Failed setting IP_MULTICAST_TTL: %s", strerror(errno));

#ifdef HAVE_STRUCT_IP_MREQN_IMR_IFINDEX
		if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &imr, sizeof(imr)))
			ERROR("Failed setting IP_MULTICAST_IF: %s", strerror(errno));
#else
		if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &ina, sizeof(ina)))
			ERROR("Failed setting IP_MULTICAST_IF: %s", strerror(errno));
#endif
	}
#ifdef AF_INET6
	else {
		int hops = ttl;

		if (setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops)))
			ERROR("Failed setting IPV6_MULTICAST_HOPS: %s", strerror(errno));

		if (setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex)))
			ERROR("Failed setting IPV6_MULTICAST_IF: %s", strerror(errno));
	}
#endif
	if (bind(sd, (struct sockaddr *)&addr, inet_addrlen(&addr)) == -1) {
		ERROR("Failed binding socket to %s", buf);
		close(sd);
		return -1;
	}

	return sd;
}

static void send_mcast(int sd, struct gr *g)
{
	char buf[BUFSZ] = { 0 };
	struct sockaddr *dest;
	socklen_t len;
	size_t seq;

	dest = (struct sockaddr *)&g->grp;
	len  = inet_addrlen(&g->grp);

	seq = g->seq;
	if (!duplicate)
		g->seq++;

	snprintf(buf, sizeof(buf), "%s%u, MC group %s ... %s%zu, %s%d",
		 MAGIC_KEY, getpid(), g->group,
		 SEQ_KEY, seq,
		 FREQ_KEY, period / 1000);
	DEBUG("Sending packet, msg: %s", buf);
	if (sendto(sd, buf, bytes, 0, dest, len) < 0) {
		ERROR("Failed sending mcast to %s: %s", g->group, strerror(errno));
		g->status[STATUS_POS] = 'E';
		g->gaps++;
	} else {
		g->bytes += bytes;
		g->count++;
		g->status[STATUS_POS] = '.';
	}
}

static void send_cb(int signo, void *arg)
{
	static int sd4 = -1;
	static int sd6 = -1;
	size_t i;

	(void)signo;
	(void)arg;

	if (sd4 == -1 && need4)
		sd4 = send_socket(AF_INET);
#ifdef AF_INET6
	if (sd6 == -1 && need6)
		sd6 = send_socket(AF_INET6);
#endif

	/* Need at least one socket to send any packet */
	if (sd4 < 0 && sd6 < 0)
		pev_exit(1);

	for (i = 0; i < group_num; i++) {
		struct gr *g = &groups[i];
		int sd;

		sd = g->grp.ss_family == AF_INET ? sd4 : sd6;
		if (sd < 0) {
			DEBUG("Skipping group %s, no available %s socket.  No address on interface?",
			      g->group, g->grp.ss_family == AF_INET ? "IPv4" : "IPv6");
			continue;
		}

		send_mcast(sd, g);
	}

	if (count > 0) {
		if (!--count)
			pev_exit(0);
	}
}

int sender_init(void)
{
	int rc;

	rc = pev_timer_add(period, send_cb, NULL);
	if (rc < 0)
		return 1;

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
