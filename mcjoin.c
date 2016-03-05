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
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_IFNAME "eth0"

#define DEBUG(fmt, ...) { if (debug)  printf(fmt, ## __VA_ARGS__); fflush(stdout); }
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
int sock = 0, count = 0;

static int usage(int code)
{
	fprintf(stderr,
		"\nUsage: %s [dqvh] [-i IFNAME] [-f FIRST] [-n NUM] [GROUP0 .. GROUPN]\n"
		"\n"
		"Options:\n"
		"  -d           Debyg output\n"
		"  -f GROUP     First Mulitcast group, e.g. 225.1.2.3\n"
		"  -n N         Total number of multicast groups, e.g. 50\n"
		"  -i IFNAME    Interface to use for multicast groups, default %s\n"
		"  -q           Quiet mode\n"
		"  -r N         Do a join/leave every N seconds\n"
		"  -v           Display program version\n"
		"  -h           This help text\n"
		"\n"
		"Mandatory arguments to long options are mandatory for short options too\n"
		"Bug report address: %-40s\n\n", __progname, DEFAULT_IFNAME, program_bug_address);

	return code;
}

static int join_group(char *iface, char *group)
{
	struct ip_mreqn mreqn;

 restart:
	if (!sock) {
		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (sock < 0) {
			fprintf(stderr, "%s: Failed opening socket(): %m\n", __func__);
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
		fprintf(stderr, "%s: \"%s\" invalid interface\n", __func__, iface);
		return 1;
	}
	DEBUG("Added iface %s, idx %d\n", iface, mreqn.imr_ifindex);

	if (inet_pton(AF_INET, group, &mreqn.imr_multiaddr) <= 0) {
		fprintf(stderr, "%s: \"%s\" invalid group address\n", __func__, group);
		return 1;
	}
	DEBUG("GROUP %#x (%s)\n", mreqn.imr_multiaddr.s_addr, group);

	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(mreqn)) < 0) {
		fprintf(stderr, "%s: IP_ADD_MEMBERSHIP: %m\n", __func__);
		return 1;
	}

	PRINT("joined group %s on %s ...\n", group, iface);

	return 0;
}

int main(int argc, char *argv[])
{
	int total = 0, restart = 0;
	int i, c, start_group;
	char iface[40], start[16], *group;
	struct in_addr start_in_addr;

	/* Default interface
	 * XXX - Should be the first, after lo, in the list at /proc/net/dev, or
	 * XXX - Iterate over /sys/class/net/.../link_mode */
	strncpy(iface, DEFAULT_IFNAME, sizeof(iface));

	while ((c = getopt(argc, argv, "df:n:i:r:qvh?")) != EOF) {
		switch (c) {
		case 'd':	/* --debug */
			debug = 1;
			break;

		case 'f':
			start_in_addr.s_addr = 0;
			if (inet_aton(optarg, &start_in_addr) == 0) {
				perror("Not a valid IP-address for first Multicast group.\n");
				return 1;
			}
			strncpy(start, optarg, sizeof(start));
			start_group = ntohl(start_in_addr.s_addr);
			DEBUG("MCSTART: %s, %#x (HOST:%#x)\n", start, start_in_addr.s_addr, start_group);
			break;

		case 'n':	/* number-of-groups */
			total = atoi(optarg);
			DEBUG("GROUPS: %d\n", total);
			break;

		case 'i':
			strncpy(iface, optarg, sizeof(iface));
			DEBUG("IFACE: %s\n", iface);
			break;

		case 'q':	/* --quiet */
			quiet = 1;
			break;

		case 'r':	/* --restart */
			restart = atoi(optarg);
			DEBUG("RESTART: %d\n", restart);
			if (restart < 1)
				restart = 1;
			break;

		case 'v':	/* --version */
			printf("%s\n", program_version);
			return 0;

		case 'h':
		case '?':
		default:
			return usage(0);
		}
	}

	/* At least one argument needed. */
	if (argc < 2)
		return usage(1);

	while (1) {
		if (!total) {
			for (i = optind; i < argc; i++) {
				start_in_addr.s_addr = 0;
				if (inet_aton(argv[i], &start_in_addr) == 0) {
					fprintf(stderr, "Invalid IPv4 group %s: %m\n", argv[i]);
					return 1;
				}
				start_group = ntohl(start_in_addr.s_addr);
				start_in_addr.s_addr = htonl(start_group);
				group = inet_ntoa(start_in_addr);

				DEBUG("Trying to join %#x (%s)\n", start_in_addr.s_addr, group);
				if (join_group(iface, group)) {
					/* Bailing out. */
					DEBUG("Bailing out...\n");
					return 1;
				}
			}
		} else {
			for (i = 0; i < total; i++) {
				start_in_addr.s_addr = htonl(start_group + i);
				group = inet_ntoa(start_in_addr);

				DEBUG("Trying to join %#x (%s)\n", start_in_addr.s_addr, group);
				if (join_group(iface, group)) {
					/* Bailing out. */
					DEBUG("Bailing out...\n");
					return 1;
				}
			}
		}

		if (!restart)
			break;

		/* If --restart=N is selected, sleep N sec before closing socket and rejoining */
		sleep(restart);

		if (sock) {
			close(sock);
			sock = 0;
			count = 0;
		}
	}

	pause();		/* Awaiting signal before exiting. */

	return 0;
}
