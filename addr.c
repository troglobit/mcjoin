/*
 * Copyright (c) 2018-2019  Joachim Nilsson <troglobit@gmail.com>
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

#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef IN_ZERONET
#define IN_ZERONET(addr) ((addr & IN_CLASSA_NET) == 0)
#endif

#ifndef IN_LOOPBACK
#define IN_LOOPBACK(addr) ((addr & IN_CLASSA_NET) == 0x7f000000)
#endif

#ifndef IN_LINKLOCAL
#define IN_LINKLOCALNETNUM 0xa9fe0000
#define IN_LINKLOCAL(addr) ((addr & IN_CLASSB_NET) == IN_LINKLOCALNETNUM)
#endif

/* Find default outbound *LAN* interface, i.e. skipping tunnels */
char *getifname(char *ifname, size_t len)
{
	uint32_t dest, gw, mask;
	char buf[256], name[17];
	char *ptr;
	FILE *fp;
	int rc, flags, cnt, use, metric, mtu, win, irtt;
	int best = 100000, found = 0;

	fp = fopen("/proc/net/route", "r");
	if (!fp)
		return NULL;

	/* Skip heading */
	ptr = fgets(buf, sizeof(buf), fp);
	if (!ptr)
		goto end;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		rc = sscanf(buf, "%16s %X %X %X %d %d %d %X %d %d %d\n",
			   name, &dest, &gw, &flags, &cnt, &use, &metric,
			   &mask, &mtu, &win, &irtt);

		if (rc < 10 || !(flags & 1)) /* IFF_UP */
			continue;

		if (dest != 0 || mask != 0)
			continue;

		if (!ifname[0] || !strncmp(ifname, "tun", 3)) {
			if (metric >= best)
				continue;

			strncpy(ifname, name, len);
			ifname[len] = 0;
			best = metric;
			found = 1;
		}
	}

end:
	fclose(fp);
	if (found)
		return ifname;

	return NULL;
}

/* Check if valid address */
static int valid_addr(struct in_addr *ina)
{
	in_addr_t addr;

	addr = ntohl(ina->s_addr);
	if (IN_ZERONET(addr) || IN_LOOPBACK(addr) || IN_LINKLOCAL(addr))
		return 0;

	return 1;
}

/* Find IPv4 address of default outbound LAN interface */
int getaddr(char *iface, struct in_addr *ina)
{
	struct ifaddrs *ifaddr, *ifa;
	char ifname[17] = { 0 };
	char buf[20] = { 0 };
	int rc = -1;

	if (!iface)
		iface = getifname(ifname, sizeof(ifname));

	rc = getifaddrs(&ifaddr);
	if (rc)
		return -1;

	for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr)
			continue;

		if (ifa->ifa_flags & IFF_LOOPBACK)
			continue;

		if (!(ifa->ifa_flags & IFF_MULTICAST))
			continue;

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		if (iface && strcmp(iface, ifa->ifa_name))
			continue;

		rc = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
				 buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
		if (!rc) {
			if (!inet_aton(buf, ina))
				continue;
			if (!valid_addr(ina))
				continue;
			break;
		}
	}
	freeifaddrs(ifaddr);

	if (rc || IN_ZERONET(ntohl(ina->s_addr)))
		return -1;

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
