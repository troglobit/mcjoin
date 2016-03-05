/* Join a multicast group and/or generate UDP test data
 *
 * Copyright (C) 2004       David Stevens <dlstevens()us!ibm!com>
 * Copyright (C) 2008-2016  Joachim Nilsson <troglobit()gmail!com>
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

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <poll.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_IFNAME "eth0"
#define DEFAULT_GROUP  "225.1.2.3"
#define DEFAULT_PORT   1234

#define DEBUG(fmt, ...) { if (debug)  printf(fmt, ## __VA_ARGS__); fflush(stdout); }
#define ERROR(fmt, ...) { fprintf(stderr, "%s:" fmt, __func__, ## __VA_ARGS__); fflush(stdout); }
#define PRINT(fmt, ...) { if (!quiet) printf(fmt, ## __VA_ARGS__); fflush(stdout); }

/* Program meta data */
extern char *__progname;
const char *program_version = "1.6-beta1";
const char *program_bug_address = "Joachim Nilsson <troglobit()gmail!com>";

/* Mode flags */
int quiet = 0;
int debug = 0;

/* getopt externals */
extern int optind;

/* socket globals */
int port = DEFAULT_PORT;
int sock = 0, count = 0;

static int join_group(char *iface, char *group)
{
	struct ip_mreqn mreqn;

 restart:
	if (!sock) {
		int val = 1;
		struct sockaddr_in sin;

		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (sock < 0) {
			ERROR("Failed opening socket(): %m\n");
			return 1;
		}

		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val));

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		sin.sin_port = htons(port);
		if (bind(sock, (struct sockaddr *)&sin, sizeof(sin))) {
			ERROR("Faild binding to socket: %m\n");
			close(sock);
			sock = 0;

			return 1;
		}
	} else {
		/* Only IP_MAX_MEMBERSHIPS (20) number of groups allowed per socket. 
		 * http://lists.freebsd.org/pipermail/freebsd-net/2003-October/001726.html
		 */
		if (++count >= IP_MAX_MEMBERSHIPS) {
			count = 0;
			sock = 0;	/* XXX: No good, losing socket... */
			goto restart;
		}
	}

	memset(&mreqn, 0, sizeof(mreqn));
	mreqn.imr_ifindex = if_nametoindex(iface);
	if (!mreqn.imr_ifindex) {
		ERROR("invalid interface: %s\n", iface);
		return 1;
	}
	DEBUG("Added iface %s, idx %d\n", iface, mreqn.imr_ifindex);

	if (inet_pton(AF_INET, group, &mreqn.imr_multiaddr) <= 0) {
		ERROR("invalid group address: %s\n", group);
		return 1;
	}
	DEBUG("GROUP %#x (%s)\n", ntohl(mreqn.imr_multiaddr.s_addr), group);

	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(mreqn)) < 0) {
		ERROR("IP_ADD_MEMBERSHIP: %m\n");
		return 1;
	}

	PRINT("joined group %s on %s ...\n", group, iface);

	return 0;
}

static int usage(int code)
{
	printf("\nUsage: %s [dhqv] [-i IFNAME] [GROUP0 .. GROUPN | GROUP+NUM]\n"
	       "\n"
	       "Options:\n"
	       "  -d           Debyg output\n"
	       "  -h           This help text\n"
	       "  -i IFNAME    Interface to use for multicast groups, default %s\n"
	       "  -p PORT      UDP port number to listen to, default: %d\n"
	       "  -q           Quiet mode\n"
	       "  -r N         Do a join/leave every N seconds\n"
	       "  -v           Display program version\n"
	       "\n"
	       "Mandatory arguments to long options are mandatory for short options too\n"
	       "Bug report address: %-40s\n\n", __progname, DEFAULT_IFNAME, DEFAULT_PORT, program_bug_address);

	return code;
}

int main(int argc, char *argv[])
{
	int total = 0, restart = 0;
	int i, c;
	char iface[IFNAMSIZ], *group, *groups[100];
	struct in_addr addr;

	/* Default interface
	 * XXX - Should be the first, after lo, in the list at /proc/net/dev, or
	 * XXX - Iterate over /sys/class/net/.../link_mode */
	strncpy(iface, DEFAULT_IFNAME, sizeof(iface));

	while ((c = getopt(argc, argv, "di:p:r:qvh")) != EOF) {
		switch (c) {
		case 'd':
			debug = 1;
			break;

		case 'h':
			return usage(0);

		case 'i':
			strncpy(iface, optarg, sizeof(iface));
			DEBUG("IFACE: %s\n", iface);
			break;

		case 'q':
			quiet = 1;
			break;

		case 'p':
			port = atoi(optarg);
			if (port < 1024 && geteuid())
				ERROR("Must be root to use priviliged ports (< 1024)\n");
			break;

		case 'r':
			restart = atoi(optarg);
			DEBUG("RESTART: %d\n", restart);
			if (restart < 1)
				restart = 1;
			break;

		case 'v':
			printf("%s\n", program_version);
			return 0;

		default:
			return usage(1);
		}
	}

	if (optind == argc)
		groups[total++] = DEFAULT_GROUP;

	/*
	 * mcjoin group+num
	 * mcjoin group0 group1 group2
	 */
	for (i = optind; i < argc; i++) {
		int j, num = 1;
		char *pos;

		group = argv[i];
		pos = strchr(group, '+');
		if (pos) {
			*pos = 0;
			num = atoi(&pos[1]);
		}

		for (j = 0; j < num; j++) {
			if (!inet_aton(group, &addr)) {
				fprintf(stderr, "%s is not a valid IPv4 multicast group\n", group);
				return usage(1);
			}

			DEBUG("Adding group %s (0x%04x) to list ...\n", group, ntohl(addr.s_addr));
			groups[total++] = strdup(group);

			/* Next group ... if any */
			addr.s_addr = htonl(ntohl(addr.s_addr) + 1);
			group = inet_ntoa(addr);
		}
	}

	while (1) {
		for (i = 0; i < total; i++) {
			if (join_group(iface, groups[i]))
				return 1;
		}

		while (1) {
			int ret;
			char buf[100];
			struct pollfd pfd = {
				.fd     = sock,
				.events = POLLIN,
			};

			ret = poll(&pfd, 1, restart ? restart * 1000 : -1);
			if (ret <= 0) {
				if (!restart)
					continue;

				close(sock);
				sock = 0;
				count = 0;
				break;
			}

			recv(sock, buf, sizeof(buf), 0);
			PRINT(".");
		}
	}

	return 0;
}
