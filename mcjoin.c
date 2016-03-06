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

#include "config.h"

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

#define BUFSZ           100
#define MAX_NUM_GROUPS  250
#define DEFAULT_IFNAME  "eth0"
#define DEFAULT_GROUP   "225.1.2.3"
#define DEFAULT_PORT    1234

#define DEBUG(fmt, ...) { if (debug)  printf(fmt, ## __VA_ARGS__); fflush(stdout); }
#define ERROR(fmt, ...) { fprintf(stderr, "%s:" fmt, __func__, ## __VA_ARGS__);    }
#define PRINT(fmt, ...) { if (!quiet) printf(fmt, ## __VA_ARGS__); fflush(stdout); }

/* Group info */
struct gr {
	int                 sd;
	size_t              count;
	char               *group;
	struct sockaddr_in  to;
};

/* Program meta data */
extern char *__progname;

/* Mode flags */
int join = 1;
int quiet = 0;
int debug = 0;
int sender = 0;
int running = 1;

/* Global data */
int period = 1000000;		/* 1 sec in micro seconds*/
int restart = 0;
size_t count = 0;
int port = DEFAULT_PORT;
unsigned char ttl = 1;

size_t group_num = 0;
struct gr groups[MAX_NUM_GROUPS];

char iface[IFNAMSIZ];
int num_joins = 0;


static int alloc_socket(int port)
{
	int sd, val;
	struct sockaddr_in sin;

	sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sd < 0) {
		ERROR("Failed opening socket(): %m\n");
		return -1;
	}

	val = 1;
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)))
		ERROR("Failed enabling SO_REUSEADDR: %m\n");

	val = 0;
	if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_ALL, &val, sizeof(val)))
		ERROR("Failed disabling IP_MULTICAST_ALL: %m\n");

	sin.sin_family      = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port        = htons(port);
	if (bind(sd, (struct sockaddr *)&sin, sizeof(sin))) {
		ERROR("Faild binding to socket: %m\n");
		close(sd);
		return -1;
	}

	return sd;
}

static int join_group(int id)
{
	int sd = alloc_socket(port); /* Index port with id if IP_MULTICAST_ALL fails */
	struct ip_mreqn mreqn;
	struct gr *gr = &groups[id];

	if (sd < 0)
		return 1;

	memset(&mreqn, 0, sizeof(mreqn));
	mreqn.imr_ifindex = if_nametoindex(iface);
	if (!mreqn.imr_ifindex) {
		ERROR("invalid interface: %s\n", iface);
		goto error;
	}
	DEBUG("Added iface %s, idx %d\n", iface, mreqn.imr_ifindex);

	if (inet_pton(AF_INET, gr->group, &mreqn.imr_multiaddr) <= 0) {
		ERROR("invalid group address: %s\n", gr->group);
		goto error;
	}
	DEBUG("GROUP %#x (%s)\n", ntohl(mreqn.imr_multiaddr.s_addr), gr->group);

	if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(mreqn)) < 0) {
		ERROR("IP_ADD_MEMBERSHIP: %m\n");
		goto error;
	}

	PRINT("joined group %s on %s ...\n", gr->group, iface);
	gr->sd = sd;
	return 0;

error:
	close(sd);
	return 1;
}

static void send_mcast(int signo)
{
	size_t i;
	char buf[BUFSZ] = { 0 };
	static int ssock = 0;
	static unsigned int counter = 1;

	if (!ssock) {
		ssock = socket(AF_INET, SOCK_DGRAM, 0);
		if (ssock < 0) {
			ERROR("Failed opening socket(): %m\n");
			return;
		}

		if (setsockopt(ssock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)))
			ERROR("Failed setting IP_MULTICAST_TTL: %m\n");
	}

	for (i = 0; i < group_num; i++) {
		socklen_t len = sizeof(groups[i].to);
		struct sockaddr *dest = (struct sockaddr *)&groups[i].to;

		snprintf(buf, sizeof(buf), "Sender PID %u, MC group %s ... count: %u", getpid(), groups[i].group, counter++);
		DEBUG("Sending packet on signal %d, msg: %s\n", signo, buf);
//		DEBUG("Sending packet to group %s\n", groups[i].group);
		sendto(ssock, buf, sizeof(buf), 0, dest, len);
	}
}

static int show_stats(void)
{
	if (join) {
		size_t i, total_count = 0;

		if (group_num > 1) {
			for (i = 0; i < group_num; i++) {
				PRINT("\nGroup %s received %zu packets", groups[i].group, groups[i].count);
				total_count += groups[i].count;
			}
		} else {
			total_count = groups[0].count;
		}

		PRINT("\nReceived total: %zu packets\n", total_count);
	}

	return 0;
}

static int loop(void)
{
	size_t i;

	if (sender) {
		struct itimerval times;

		for (i = 0; i < group_num; i++) {
			char *group = groups[i].group;
			struct sockaddr_in *sin = &groups[i].to;

			sin->sin_family      = AF_INET;
			sin->sin_addr.s_addr = inet_addr(group);
			sin->sin_port        = htons(port); /* Index port with i if IP_MULTICAST_ALL fails */
		}
		signal(SIGALRM, send_mcast);

		times.it_value.tv_sec     = 1;	/* wait a bit for system to "stabilize"  */
		times.it_value.tv_usec    = 0;	/* tv_sec or tv_usec cannot be both zero */
		times.it_interval.tv_sec  = (time_t)(period / 1000000);
		times.it_interval.tv_usec =   (long)(period % 1000000);
		setitimer(ITIMER_REAL, &times, NULL);
	}

	while (join && running) {
		for (i = 0; i < group_num; i++) {
			if (join_group(i))
				return 1;
		}

		while (running) {
			int ret;
			char buf[BUFSZ];
			struct pollfd pfd[MAX_NUM_GROUPS];

			/* One group per socket */
			for (i = 0; i < group_num; i++) {
				pfd[i].fd = groups[i].sd;
				pfd[i].events = POLLIN;
				pfd[i].revents = 0;
			}

			ret = poll(pfd, group_num, restart ? restart * 1000 : -1);
			if (ret <= 0) {
				if (ret < 0 || !restart)
					continue;

				for (i = 0; i < group_num; i++) {
					close(groups[i].sd);
					groups[i].sd = 0;
					num_joins = 0;
				}
				break;
			}

			for (i = 0; i < group_num; i++) {
				if (pfd[i].revents) {
					int pid;
					ssize_t bytes;

					bytes = recv(pfd[i].fd, buf, sizeof(buf), MSG_DONTWAIT);
					if (bytes > 0) {
						buf[bytes] = 0;
						groups[i].count++;

						pid = atoi(buf);
						DEBUG("Count %zu, Our PID %d Got PID: %d, group %s msg: %s\n",
						      groups[i].count, getpid(), pid, groups[i].group, buf);
						if (pid != getpid())
							PRINT(".");
					}

					pfd[i].revents = 0;
				}
			}

			if (count > 0) {
				size_t total = count * group_num;

				for (i = 0; i < group_num; i++) {
					if (groups[i].count >= count)
						total--;
				}

				if (!total) {
					running = 0;
					break;
				}
			}

			DEBUG("\n");
		}
	}

	while (running) {
		poll(NULL, 0, -1);
		if (count > 0) {
			count--;
			if (!count)
				return 0;
		}
	}

	DEBUG("Leaving main loop\n");

	return show_stats();
}

static void exit_loop(int signo)
{
	DEBUG("We got signal! (signo: %d)\n", signo);
	running = 0;
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
	       "Bug report address: %-40s\n"
	       "Project homepage: %s\n\n", __progname, DEFAULT_IFNAME, DEFAULT_PORT, PACKAGE_BUGREPORT, PACKAGE_URL);

	return code;
}

int main(int argc, char *argv[])
{
	int i, c;
	extern int optind;

	/* Default interface
	 * XXX - Should be the first, after lo, in the list at /proc/net/dev, or
	 * XXX - Iterate over /sys/class/net/.../link_mode */
	strncpy(iface, DEFAULT_IFNAME, sizeof(iface));

	for (i = 0; i < MAX_NUM_GROUPS; i++)
		memset(&groups[i], 0, sizeof(groups[0]));

	while ((c = getopt(argc, argv, "c:di:jp:qr:st:vh")) != EOF) {
		switch (c) {
		case 'c':
			count = (size_t)atoi(optarg);
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
			printf("%s\n", PACKAGE_VERSION);
			return 0;

		default:
			return usage(1);
		}
	}

	if (optind == argc)
		groups[group_num++].group = strdup(DEFAULT_GROUP);

	/*
	 * mcjoin group+num
	 * mcjoin group0 group1 group2
	 */
	for (i = optind; i < argc; i++) {
		int j, num = 1;
		char *pos, *group;
		struct in_addr addr;

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
			groups[group_num++].group = strdup(group);

			/* Next group ... if any */
			addr.s_addr = htonl(ntohl(addr.s_addr) + 1);
			group = inet_ntoa(addr);
		}
	}

	/*
	 * Shared signal handlers between sender and receiver
	 */
	signal(SIGINT, exit_loop);
	signal(SIGHUP, exit_loop);
	signal(SIGTERM, exit_loop);

	return loop();
}
