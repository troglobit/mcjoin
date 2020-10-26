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

#include <netdb.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "addr.h"
#include "log.h"

const char *inet_address(inet_addr_t *ss, char *buf, size_t len)
{
	struct sockaddr_in *sin;

#ifdef AF_INET6
	if (ss->ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;
		return inet_ntop(AF_INET6, &sin6->sin6_addr, buf, len);
	}
#endif

	sin = (struct sockaddr_in *)ss;
	return inet_ntop(AF_INET, &sin->sin_addr, buf, len);
}

socklen_t inet_addrlen(inet_addr_t *ss)
{
#ifdef AF_INET6
	if (ss->ss_family == AF_INET6)
		return sizeof(struct sockaddr_in6);
#endif
	if (ss->ss_family == AF_INET)
		return sizeof(struct sockaddr_in);
	return 0;
}

void inet_addr_set_port(inet_addr_t *ss, short port)
{
	struct sockaddr_in *sin;

#ifdef AF_INET6
	if (ss->ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;

		sin6->sin6_port = port;
		return;
	}
#endif

	sin = (struct sockaddr_in *)ss;
	sin->sin_port = port;
}

short inet_addr_get_port(inet_addr_t *ss)
{
	struct sockaddr_in *sin;

#ifdef AF_INET6
	if (ss->ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;
		return sin6->sin6_port;
	}
#endif

	sin = (struct sockaddr_in *)ss;
	return sin->sin_port;
}

/* The BSD's or SVR4 systems like Solaris don't have /proc/net/route */
static char *altdefault(char *iface, size_t len)
{
	char buf[256];
	FILE *fp;

	fp = popen("netstat -rn", "r");
	if (!fp)
		return NULL;

	while (fgets(buf, sizeof(buf), fp)) {
		char *token;

		if (strncmp(buf, "default", 7) && strncmp(buf, "0.0.0.0", 7))
			continue;

		token = strtok(buf, " \t\n");
		while (token) {
			if (if_nametoindex(token)) {
				strncpy(iface, token, len);
				pclose(fp);

				return iface;
			}

			token = strtok(NULL, " \t\n");
		}
	}

	pclose(fp);
	return NULL;
}

/* Find default outbound *LAN* interface, i.e. skipping tunnels */
char *ifdefault(char *iface, size_t len)
{
	uint32_t dest, gw, mask;
	char buf[256], ifname[17];
	char *ptr;
	FILE *fp;
	int rc, flags, cnt, use, metric, mtu, win, irtt;
	int best = 100000, found = 0;

	fp = fopen("/proc/net/route", "r");
	if (!fp)
		return altdefault(iface, len);

	/* Skip heading */
	ptr = fgets(buf, sizeof(buf), fp);
	if (!ptr)
		goto end;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		rc = sscanf(buf, "%16s %X %X %X %d %d %d %X %d %d %d\n",
			   ifname, &dest, &gw, &flags, &cnt, &use, &metric,
			   &mask, &mtu, &win, &irtt);

		if (rc < 10 || !(flags & 1)) /* IFF_UP */
			continue;

		if (dest != 0 || mask != 0)
			continue;

		if (!iface[0] || !strncmp(iface, "tun", 3)) {
			if (metric >= best)
				continue;

			strncpy(iface, ifname, len);
			iface[len] = 0;
			best = metric;
			found = 1;

			DEBUG("Found default intefaces %s", iface);
		}
	}

end:
	fclose(fp);
	if (found)
		return iface;

	return NULL;
}

/* XXX: old IPv4-only address validation, fixme!
 * http://osr600doc.xinuos.com/en/SDK_netapi/sockC.PortIPv4appIPv6.html
 * http://www.kame.net/newsletter/19980604/
 */
#if 0
/* Check if valid address */
static int valid_addr(struct in_addr *ina)
{
	in_addr_t addr;

	addr = ntohl(ina->s_addr);
	if (IN_ZERONET(addr) || IN_LOOPBACK(addr) || IN_LINKLOCAL(addr))
		return 0;

	return 1;
}
#endif

/* Find IP address of default outbound LAN interface */
int ifinfo(char *iface, inet_addr_t *addr, int family)
{
	struct ifaddrs *ifaddr, *ifa;
	char ifname[17] = { 0 };
	char buf[INET_ADDRSTR_LEN] = { 0 };
	int rc = -1;

	if (!iface || !iface[0])
		iface = ifdefault(ifname, sizeof(ifname));
	if (!iface)
		return -2;

	rc = getifaddrs(&ifaddr);
	if (rc == -1)
		return -3;

	rc = -1; /* Return -1 if iface with family is not found */
	for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr)
			continue;

		if (!(ifa->ifa_flags & IFF_MULTICAST))
			continue;

		if (ifa->ifa_addr->sa_family != family)
			continue;

		if (iface && strcmp(iface, ifa->ifa_name))
			continue;

		DEBUG("Found %s addr %s", ifa->ifa_name,
		      inet_address((inet_addr_t *)ifa->ifa_addr, buf, sizeof(buf)));
		*addr = *(inet_addr_t *)ifa->ifa_addr;
#if 0 /* XXX: old IPv4-only address validation, fixme! */
		if (family == AF_INET) {
			if (!inet_aton(buf, ina))
				continue;
			if (!valid_addr(ina))
				continue;
		}
#endif
		rc = if_nametoindex(ifa->ifa_name);
		DEBUG("Valid iface %s, ifindex %d, addr %s", ifa->ifa_name, rc, buf);
		break;
	}
	freeifaddrs(ifaddr);

	return rc;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
