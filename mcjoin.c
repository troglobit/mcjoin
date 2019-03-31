/* Join a multicast group and/or generate UDP test data
 *
 * Copyright (C) 2004       David Stevens <dlstevens()us!ibm!com>
 * Copyright (C) 2008-2018  Joachim Nilsson <troglobit()gmail!com>
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
#include <ifaddrs.h>
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
#define DEFAULT_GROUP   "225.1.2.3"
#define DEFAULT_PORT    1234
#define MAGIC_KEY       "Sender PID "

/* Esc[?25l (lower case L)    - Hide Cursor */
#define hidecursor()          fputs ("\e[?25l", stdout)
/* Esc[?25h (lower case H)    - Show Cursor */
#define showcursor()          fputs ("\e[?25h", stdout)

#define DEBUG(fmt, ...) { if (debug)  printf(fmt "\n", ## __VA_ARGS__); fflush(stdout); }
#define ERROR(fmt, ...) { fprintf(stderr, fmt "\n", ## __VA_ARGS__);    }
#define PRINT(fmt, ...) { if (!quiet) printf(fmt "\n", ## __VA_ARGS__); fflush(stdout); }

#ifndef IN_LINKLOCAL
#define IN_LINKLOCALNETNUM	0xa9fe0000
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

#ifdef  HAVE_IPV6_MULTICAST_HOST
#define INET_ADDRSTR_LEN  INET6_ADDRSTRLEN
#else
#define INET_ADDRSTR_LEN  INET_ADDRSTRLEN
#endif
typedef struct sockaddr_storage inet_addr_t;

/* Group info */
struct gr {
	int          sd;
	size_t       count;
	char        *source;
	char        *group;
	inet_addr_t  src;
	inet_addr_t  grp;	/* to */
};

/* Mode flags */
int join = 1;
int quiet = 0;
int debug = 0;
int sender = 0;
int running = 1;

/* Global data */
int period = 100000;		/* 100 msec in micro seconds*/
int restart = 0;
size_t count = 0;
int port = DEFAULT_PORT;
unsigned char ttl = 1;
char *ident = PACKAGE_NAME;

size_t group_num = 0;
struct gr groups[MAX_NUM_GROUPS];

char iface[IFNAMSIZ + 1];
int num_joins = 0;

char *getifname(char *ifname, size_t len);
int getaddr(char *iface, struct in_addr *ina);

static const char *convert_address(inet_addr_t *ss, char *buf, size_t len)
{
	struct sockaddr_in *sin;

#ifdef HAVE_IPV6_MULTICAST_HOST
	if (ss->ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;
		return inet_ntop(AF_INET6, &sin6->sin6_addr, buf, len);
	}
#endif

	sin = (struct sockaddr_in *)ss;
	return inet_ntop(AF_INET, &sin->sin_addr, buf, len);
}

static int alloc_socket(inet_addr_t group)
{
	int sd, val;

	sd = socket(group.ss_family, SOCK_DGRAM, IPPROTO_UDP);
	if (sd < 0) {
		ERROR("Failed opening socket(): %s", strerror(errno));
		return -1;
	}

	val = 1;
#ifdef SO_REUSEPORT
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val)))
		ERROR("Failed enabling SO_REUSEPORT: %s", strerror(errno));
#endif
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)))
		ERROR("Failed enabling SO_REUSEADDR: %s", strerror(errno));

	if (setsockopt(sd, SOL_IP, IP_PKTINFO, &val, sizeof(val)))
		ERROR("Failed enabling IP_PKTINFO: %s", strerror(errno));

	val = 0;
	if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_ALL, &val, sizeof(val)))
		ERROR("Failed disabling IP_MULTICAST_ALL: %s", strerror(errno));

	if (bind(sd, (struct sockaddr *)&group, sizeof(group))) {
		ERROR("Faild binding to socket: %s", strerror(errno));
		close(sd);
		return -1;
	}

	return sd;
}

static int join_group(struct gr *sg)
{
	struct group_source_req gsr;
	struct group_req gr;
	char src[INET_ADDRSTR_LEN] = "*";
	char grp[INET_ADDRSTR_LEN];
	size_t len;
	void *arg;
	int ifindex;
	int sd, op;

	/* Index port with id if IP_MULTICAST_ALL fails */
	sd = alloc_socket(sg->grp);
	if (sd < 0)
		return 1;

	ifindex = if_nametoindex(iface);
	if (!ifindex) {
		ERROR("invalid interface: %s", iface);
		goto error;
	}
	DEBUG("Added iface %s, idx %d", iface, ifindex);

	if (sg->source) {
		gsr.gsr_interface  = ifindex;
		gsr.gsr_source     = sg->src;
		gsr.gsr_group      = sg->grp;
		op                 = MCAST_JOIN_SOURCE_GROUP;
		arg                = &gsr;
		len                = sizeof(gsr);
	} else {
		gr.gr_interface    = ifindex;
		gr.gr_group        = sg->grp;
		op                 = MCAST_JOIN_GROUP;
		arg                = &gr;
		len                = sizeof(gr);
	}

	if (sg->source)
		convert_address(&sg->src, src, sizeof(src));
	convert_address(&sg->grp, grp, sizeof(grp));
	DEBUG("Joining group (%s,%s) on %s", src, grp, iface);

	if (setsockopt(sd, IPPROTO_IP, op, arg, len)) {
		ERROR("Failed %s group (%s,%s) on sd %d ... %d: %s",
		      src, grp, "joining", sd, errno, strerror(errno));
		goto error;
	}

	return 0;

	PRINT("joined group %s on %s ...", sg->group, iface);
	sg->sd = sd;

	return 0;

error:
	close(sd);
	return 1;
}

static void send_mcast(int signo)
{
	static unsigned int counter = 1;
	static int ssock = 0;
	char buf[BUFSZ] = { 0 };
	size_t i;

	if (!ssock) {
		struct in_addr addr = {
			.s_addr = INADDR_ANY,
		};

		if (getaddr(iface, &addr)) {
			ERROR("Failed locating (a valid address on) %s: %s", iface, strerror(errno));
			return;
		}
		DEBUG("Sending on iface %s addr %s", iface, inet_ntoa(addr));

		ssock = socket(AF_INET, SOCK_DGRAM, 0);
		if (ssock < 0) {
			ERROR("Failed opening socket(): %s", strerror(errno));
			return;
		}

		if (setsockopt(ssock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)))
			ERROR("Failed setting IP_MULTICAST_TTL: %s", strerror(errno));

		if (setsockopt(ssock, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)))
			ERROR("Failed setting IP_MULTICAST_IF: %s", strerror(errno));
	}

	for (i = 0; i < group_num; i++) {
		struct sockaddr *dest = (struct sockaddr *)&groups[i].grp;
		socklen_t len = sizeof(groups[i].grp);

		snprintf(buf, sizeof(buf), "%s%u, MC group %s ... count: %u", MAGIC_KEY, getpid(), groups[i].group, counter++);
		DEBUG("Sending packet on signal %d, msg: %s", signo, buf);
		if (sendto(ssock, buf, sizeof(buf), 0, dest, len) < 0)
			ERROR("Failed sending mcast packet: %s", strerror(errno));
	}
}

struct in_pktinfo *find_pktinfo(struct msghdr *msgh)
{
	struct cmsghdr *cmsg;

	for (cmsg = CMSG_FIRSTHDR(msgh); cmsg; cmsg = CMSG_NXTHDR(msgh, cmsg)) {
		if (cmsg->cmsg_level != SOL_IP || cmsg->cmsg_type != IP_PKTINFO)
			continue;

		return (struct in_pktinfo *)CMSG_DATA(cmsg);
	}

	return NULL;
}

static void progress(void)
{
	const char *style = ".oOOo.";
	static unsigned int i = 0;
	size_t num = 6;

	if (quiet)
		return;

	if (!(i % num))
		printf(".");

	putchar(style[i++ % num]);
	putchar('\b');

	fflush(stdout);
}

/*
 * rcvmsg() wrapper which uses out-of-band info to verify expected
 * destination address (multicast group)
 */
static ssize_t recv_mcast(int id)
{
	struct sockaddr_storage src;
	struct in_pktinfo *ipi;
	struct msghdr msgh;
	struct iovec iov[1];
	ssize_t bytes;
	char cmbuf[0x100];
	char buf[BUFSZ];

	iov[0].iov_base = buf;
	iov[0].iov_len  = sizeof(buf);

	memset(&msgh, 0, sizeof(msgh));
	msgh.msg_name       = &src;
	msgh.msg_namelen    = sizeof(src);
	msgh.msg_iov        = iov;
	msgh.msg_iovlen     = NELEMS(iov);
	msgh.msg_control    = cmbuf;
	msgh.msg_controllen = sizeof(cmbuf);

	bytes = recvmsg(groups[id].sd, &msgh, MSG_DONTWAIT);
	if (bytes < 0)
		return -1;

	ipi = find_pktinfo(&msgh);
	if (ipi) {
		char *ptr;
		int pid = 0;

		buf[bytes] = 0;
		ptr = strstr(buf, MAGIC_KEY);
		if (ptr)
			pid = atoi(ptr + strlen(MAGIC_KEY));

		DEBUG("Count %5zu, our PID %d, sender PID %d, group %s msg: %s", groups[id].count, getpid(), pid, groups[id].group, buf);
		if (pid != getpid()) {
			char *dst = inet_ntoa(ipi->ipi_addr);

			if (strcmp(dst, groups[id].group)) {
				ERROR("Packet for group %s received on wrong socket, expected group %s.", dst, groups[id].group);
				return -1;
			}

			groups[id].count++;
			progress();
			return 0;
		}
	}

	return -1;
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

		PRINT("\nReceived total: %zu packets", total_count);
	}

	return 0;
}

static int loop(void)
{
	struct sigaction sa = {
		.sa_flags = SA_RESTART,
		.sa_handler = send_mcast,
	};
	size_t i;

	if (sender) {
		struct itimerval times;

		sigaction(SIGALRM, &sa, NULL);

		times.it_value.tv_sec     = 1;	/* wait a bit for system to "stabilize"  */
		times.it_value.tv_usec    = 0;	/* tv_sec or tv_usec cannot be both zero */
		times.it_interval.tv_sec  = (time_t)(period / 1000000);
		times.it_interval.tv_usec =   (long)(period % 1000000);
		setitimer(ITIMER_REAL, &times, NULL);
	}

	while (join && running) {
		for (i = 0; i < group_num; i++) {
			if (join_group(&groups[i]))
				return 1;
		}

		hidecursor();
		while (running) {
			struct pollfd pfd[MAX_NUM_GROUPS];
			int ret;

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
					recv_mcast(i);
					pfd[i].revents = 0;
				}
			}

			if (count > 0) {
				size_t total = count * group_num;

				for (i = 0; i < group_num; i++)
					total -= groups[i].count;

				if (total <= 0) {
					running = 0;
					break;
				}
			}

			DEBUG("");
		}
		showcursor();
	}

	while (running) {
		pause();	/* Let signal handler(s) do their job */
		if (count > 0) {
			if (!--count)
				break;
		}
	}

	DEBUG("Leaving main loop");

	return show_stats();
}

static void exit_loop(int signo)
{
	DEBUG("We got signal! (signo: %d)", signo);
	running = 0;
}

static int usage(int code)
{
	printf("Usage: %s [-dhjqsv] [-c COUNT] [-i IFACE] [-p PORT] [-r SEC] [-t TTL]\n"
	       "              [GROUP0 .. GROUPN | GROUP+NUM]\n"
	       "Options:\n"
	       "  -c COUNT     Stop sending/receiving after COUNT number of packets\n"
	       "  -d           Debug output\n"
	       "  -h           This help text\n"
	       "  -i IFACE     Interface to use for sending/receiving multicast, default: %s\n"
	       "  -j           Join groups, default unless acting as sender\n"
	       "  -p PORT      UDP port number to send/listen to, default: %d\n"
	       "  -q           Quiet mode\n"
	       "  -r SEC       Do a join/leave every SEC seconds (backwards compat. option)\n"
	       "  -s           Act as sender, sends packets to select groups, default: no\n"
	       "  -t TTL       TTL to use when sending multicast packets, default: 1\n"
	       "  -v           Display program version\n"
	       "\n"
	       "Bug report address : %-40s\n"
	       "Project homepage   : %s\n", ident, iface, DEFAULT_PORT, PACKAGE_BUGREPORT, PACKAGE_URL);

	return code;
}

static char *progname(char *arg0)
{
       char *nm;

       nm = strrchr(arg0, '/');
       if (nm)
	       nm++;
       else
	       nm = arg0;

       return nm;
}

int main(int argc, char *argv[])
{
	struct sigaction sa = {
		.sa_flags = SA_RESTART,
		.sa_handler = exit_loop,
	};
	extern int optind;
	size_t len;
	int i, c;

	getifname(iface, sizeof(iface));
	for (i = 0; i < MAX_NUM_GROUPS; i++)
		memset(&groups[i], 0, sizeof(groups[0]));

	ident = progname(argv[0]);
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
			len = strlen(optarg);
			if (len >= sizeof(iface)) {
				ERROR("Too long interface name, max %zd chars.", sizeof(iface) - 1);
				return 1;
			}
			strncpy(iface, optarg, sizeof(iface));
			iface[len] = 0;
			DEBUG("IFACE: %s", iface);
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
				ERROR("Must be root to use priviliged ports (< 1024)");
			break;

		case 'r':
			restart = atoi(optarg);
			DEBUG("RESTART: %d", restart);
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
		char *pos, *group, *source = NULL;
		int j, num = 1;

		group = argv[i];
		pos = strchr(group, '+');
		if (pos) {
			*pos = 0;
			num = atoi(&pos[1]);
		}

		pos = strchr(group, ',');
		if (pos) {
			*pos++ = 0;
			source = strdup(group);
			group  = pos;
		}

		if (num < 1 || (num + group_num) >= NELEMS(groups)) {
			ERROR("Invalid number of groups given (%d), or max (%zd) reached.", num, NELEMS(groups));
			return usage(1);
		}

		for (j = 0; j < num && group_num < NELEMS(groups); j++) {
			struct in_addr addr;

			if (!inet_aton(group, &addr)) {
				ERROR("%s is not a valid IPv4 multicast group", group);
				return usage(1);
			}

			DEBUG("Adding (S,G) %s,%s to list ...", source, group);
			groups[group_num].source  = source;
			groups[group_num++].group = strdup(group);

			/* Next group ... */
			addr.s_addr = htonl(ntohl(addr.s_addr) + 1);
			group = inet_ntoa(addr);
		}
	}

	for (i = 0; i < (int)group_num; i++) {
		struct sockaddr_in *grp = (struct sockaddr_in *)&groups[i].grp;
		struct sockaddr_in *src = (struct sockaddr_in *)&groups[i].src;

		inet_pton(AF_INET, groups[i].group, &grp->sin_addr);
		grp->sin_family = AF_INET;
		grp->sin_port   = htons(port);

		if (groups[i].source) {
			inet_pton(AF_INET, groups[i].source, &src->sin_addr);
			src->sin_family = AF_INET;
			src->sin_port   = htons(port);
		}
	}

	/*
	 * Shared signal handlers between sender and receiver
	 */
	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGHUP,  &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	return loop();
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
