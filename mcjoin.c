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
#include <signal.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define BUFSZ          100
#define MAX_NUM_GROUPS 100
#define DEFAULT_IFNAME "eth0"
#define DEFAULT_GROUP  "225.1.2.3"
#define DEFAULT_PORT   1234

#define DEBUG(fmt, ...) { if (debug)  printf(fmt, ## __VA_ARGS__); fflush(stdout); }
#define ERROR(fmt, ...) { fprintf(stderr, "%s:" fmt, __func__, ## __VA_ARGS__);    }
#define PRINT(fmt, ...) { if (!quiet) printf(fmt, ## __VA_ARGS__); fflush(stdout); }

/* Program meta data */
extern char *__progname;
const char *program_version = "2.0-beta2";
const char *program_bug_address = "Joachim Nilsson <troglobit()gmail!com>";

/* Mode flags */
int join = 1;
int quiet = 0;
int debug = 0;
int sender = 0;
int period = 1000000;		/* 1 sec in micro seconds*/
int restart = 0;

/* getopt externals */
extern int optind;

/* shared socket settings */
int count = -1;
int port = DEFAULT_PORT;

/* sender socket */
int ssock = 0;
unsigned char ttl = 1;
int ton = 0;
struct sockaddr_in to[MAX_NUM_GROUPS];

/* receiver socket */
char iface[IFNAMSIZ];
int rsock = 0, num_joins = 0;


static int join_group(char *iface, char *group)
{
	struct ip_mreqn mreqn;

 restart:
	if (!rsock) {
		int val = 1;
		struct sockaddr_in sin;

		rsock = socket(AF_INET, SOCK_DGRAM, 0);
		if (rsock < 0) {
			ERROR("Failed opening socket(): %m\n");
			return 1;
		}

		if (setsockopt(rsock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)))
			ERROR("Failed enabling SO_REUSEADDR: %m\n");

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		sin.sin_port = htons(port);
		if (bind(rsock, (struct sockaddr *)&sin, sizeof(sin))) {
			ERROR("Faild binding to socket: %m\n");
			close(rsock);
			rsock = 0;

			return 1;
		}
	} else {
		/* Only IP_MAX_MEMBERSHIPS (20) number of groups allowed per socket.
		 * http://lists.freebsd.org/pipermail/freebsd-net/2003-October/001726.html
		 */
		if (++num_joins >= IP_MAX_MEMBERSHIPS) {
			num_joins = 0;
			rsock = 0;	/* XXX: No good, losing socket... */
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

	if (setsockopt(rsock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(mreqn)) < 0) {
		ERROR("IP_ADD_MEMBERSHIP: %m\n");
		return 1;
	}

	PRINT("joined group %s on %s ...\n", group, iface);

	return 0;
}

void send_mcast(int signo __attribute__((unused)))
{
	int i;
	char buf[BUFSZ] = { 0 };
	static int counter = 1;

	if (!ssock) {
		ssock = socket(AF_INET, SOCK_DGRAM, 0);
		if (ssock < 0) {
			ERROR("Failed opening socket(): %m\n");
			return;
		}

		if (setsockopt(ssock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)))
			ERROR("Failed setting IP_MULTICAST_TTL: %m\n");
	}

	snprintf(buf, sizeof(buf), "%u All your base are belong to us ... count: %d", getpid(), counter++);
	DEBUG("Sending packet: %s\n", buf);
	for (i = 0; i < ton; i++)
		sendto(ssock, buf, sizeof(buf), 0, (struct sockaddr *)&to[i], sizeof(to[0]));
}

int loop(int total, char *groups[])
{
	int i;

	if (sender) {
		struct itimerval times;

		for (i = 0; i < total; i++) {
			to[i].sin_family      = AF_INET;
			to[i].sin_addr.s_addr = inet_addr(groups[i]);
			to[i].sin_port        = htons(port);
		}
		ton = total;
		signal(SIGALRM, send_mcast);

		times.it_value.tv_sec     = 1;	/* wait a bit for system to "stabilize"  */
		times.it_value.tv_usec    = 0;	/* tv_sec or tv_usec cannot be both zero */
		times.it_interval.tv_sec  = (time_t)(period / 1000000);
		times.it_interval.tv_usec =   (long)(period % 1000000);
		setitimer(ITIMER_REAL, &times, NULL);
	}

	while (join) {
		for (i = 0; i < total; i++) {
			if (join_group(iface, groups[i]))
				return 1;
		}

		while (1) {
			int ret;
			char buf[BUFSZ];
			struct pollfd pfd = {
				.fd     = rsock,
				.events = POLLIN,
			};

			ret = poll(&pfd, 1, restart ? restart * 1000 : -1);
			if (ret <= 0) {
				if (ret < 0 || !restart)
					continue;

				close(rsock);
				rsock = 0;
				num_joins = 0;
				break;
			}

			recv(rsock, buf, sizeof(buf), 0);
			ret = atoi(buf);
			DEBUG("OUR PID %d GOT PID: %d BUF: %s\n", getpid(), ret, buf);
			if (ret != getpid()) {
				PRINT(".");
				if (count > 0) {
					count--;
					if (!count)
						return 0;
				}
			}
		}
	}

	while (1) {
		poll(NULL, 0, -1);
		if (count > 0) {
			count--;
			if (!count)
				return 0;
		}
	}

	return 0;
}

static int usage(int code)
{
	printf("\nUsage: %s [dhjqsv] [-c COUNT] [-i IFACE] [-p PORT] [-r SEC] [-t TTL]\n"
	       "              [GROUP0 .. GROUPN | GROUP+NUM]\n"
	       "\n"
	       "Options:\n"
	       "  -c COUNT     Stop after sending/receiving COUNT number of packets\n"
	       "  -d           Debug output\n"
	       "  -h           This help text\n"
	       "  -i IFACE     Interface to use for sending/receiving multicast, default: %s\n"
	       "  -j           Join groups, default unless acting as sender\n"
	       "  -p PORT      UDP port number to listen to, default: %d\n"
	       "  -q           Quiet mode\n"
	       "  -r SEC       Do a join/leave every SEC seconds (backwards compat. option)\n"
	       "  -s           Act as sender, sends packets to select groups, default: no\n"
	       "  -t TTL       TTL to use when sending multicast packets, default: 1\n"
	       "  -v           Display program version\n"
	       "\n"
	       "Mandatory arguments to long options are mandatory for short options too\n"
	       "Bug report address: %-40s\n\n", __progname, DEFAULT_IFNAME, DEFAULT_PORT, program_bug_address);

	return code;
}

int main(int argc, char *argv[])
{
	int i, c, total = 0;
	char *group, *groups[100];
	struct in_addr addr;

	/* Default interface
	 * XXX - Should be the first, after lo, in the list at /proc/net/dev, or
	 * XXX - Iterate over /sys/class/net/.../link_mode */
	strncpy(iface, DEFAULT_IFNAME, sizeof(iface));

	while ((c = getopt(argc, argv, "c:di:jp:qr:st:vh")) != EOF) {
		switch (c) {
		case 'c':
			count = atoi(optarg);
			break;

		case 'd':
			debug = 1;
			break;

		case 'h':
			return usage(0);

		case 'i':
			strncpy(iface, optarg, sizeof(iface));
			DEBUG("IFACE: %s\n", iface);
			break;

		case 'j':
			join++;
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

		case 's':
			sender = 1;
			join--;
			break;

		case 't':
			ttl = atoi(optarg);
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

	return loop(total, groups);
}
