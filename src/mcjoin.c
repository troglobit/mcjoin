/* Join a multicast group and/or generate UDP test data
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

#include <arpa/inet.h>		/* inet_pton() on Solaris */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/resource.h>

#include "addr.h"
#include "log.h"
#include "mcjoin.h"
#include "screen.h"

/* Mode flags */
int old = 0;
int join = 1;
int debug = 0;
int hastty = 1;
int foreground = 1;

/* Global data */
int period = 100000;		/* 100 msec in micro seconds*/
int width = 80;
int height = 24;
size_t bytes = 100;
size_t count = 0;
int port = DEFAULT_PORT;
unsigned char ttl = 1;
char *ident = PACKAGE_NAME;
time_t start;

int need4 = 0;
int need6 = 0;

size_t group_num = 0;
struct gr groups[MAX_NUM_GROUPS];

char iface[IFNAMSIZ + 1];

static char spin(struct gr *g)
{
	const char *spinner = "|/-\\";
	size_t num = strlen(spinner);
	char act;

	/* spin on activity only */
	act = spinner[g->spin % num];
	if (g->status[STATUS_POS] == '.')
		g->spin++;

	return act;
}

void plotter_show(int signo)
{
	int swidth;
	int spos;
	char act = 0;
	size_t i;

	(void)signo;

	if (old) {
		if (!hastty)
			return;

		for (i = 0; i < group_num; i++) {
			struct gr *g = &groups[i];

			if (g->status[STATUS_POS] != ' ')
				act = g->status[STATUS_POS];
		}

		if (act)
			progress();

		return;
	}

	swidth = width - 50;
	if (swidth > STATUS_HISTORY)
		swidth = STATUS_HISTORY;
	spos = STATUS_HISTORY - swidth;

	for (i = 0; i < group_num; i++) {
		struct gr *g = &groups[i];
		char sgbuf[35];

		gotoxy(0, GROUP_ROW + i);
		act = spin(g);

		snprintf(sgbuf, sizeof(sgbuf), "%s,%s", g->source ? g->source : "*", g->group);
		fprintf(stderr, "%-31s  %c [%s] %13zu", sgbuf, act, &g->status[spos], g->count);
	}
}

static char *uptime(time_t up)
{
	static char buf[42];
	time_t day, hour, min, sec;

	day = up / (24 * 60 * 60);
	 up = up % (24 * 60 * 60);

	hour = up / (60 * 60);
	  up = up % (60 * 60);

	min = up / 60;
	 up = up % 60;

	sec = up;

	if (day)
		snprintf(buf, sizeof(buf), "%ldd %02ldh %02ldm %02lds", day, hour, min, sec);
	else if (hour)
		snprintf(buf, sizeof(buf), "%ldh %02ldm %02lds", hour, min, sec);
	else if (min)
		snprintf(buf, sizeof(buf), "%ldm %02lds", min, sec);
	else if (sec)
		snprintf(buf, sizeof(buf), "%lds", sec);

	return buf;
}

static void show_stats(void)
{
	size_t i, total_count = 0;
	int gwidth = 0;
	time_t now;

	for (i = 0; i < group_num; i++) {
		if ((int)strlen(groups[i].group) > gwidth)
			gwidth = (int)strlen(groups[i].group);
	}

	/* Reset log in case of user scrolling to show stats */
	log_scroll(0);

	for (i = 0; i < group_num; i++) {
		if (join)
			PRINT("Group %-*s received %zu packets, gaps: %zu", gwidth,
			      groups[i].group, groups[i].count, groups[i].gaps);
		else
			PRINT("Sent %zu packets to group %-*s errors: %d",
			      groups[i].count, gwidth, groups[i].group,
			      groups[i].gaps);

		total_count += groups[i].count;
	}

	if (join)
		PRINT("\nReceived total: %zu packets", total_count);
	else
		PRINT("\nSent total: %zu packets", total_count);

	now = time(NULL);
	PRINT("Uptime: %s", uptime(now - start));
}

static void redraw(int signo)
{
	const char *howto = "ctrl-c to exit";
	const char *title;

	if (old || !foreground)
		return;

	if (signo)
		ttsize(&width, &height);

	if (!join)
		title = "mcjoin :: sending multicast";
	else
		title = "mcjoin :: receiving multicast";

	cls();
	gotoxy((width - strlen(title)) / 2, TITLE_ROW);
	fprintf(stderr, "\e[1m%s\e[0m", title);
	gotoxy((width - strlen(howto)) / 2, HOSTDATE_ROW);
	fprintf(stderr, "\e[2m%s\e[0m", howto);
	gotoxy(0, HEADING_ROW);
	fprintf(stderr, "\e[7m%-31s    Plotter%*s      Packets\e[0m", "Source,Group", width - 55, " ");

	gotoxy(0, LOGHEADING_ROW); /* Thu Nov  5 09:08:59 2020 */
	fprintf(stderr, "\e[7m%-24s  Log%*s\e[0m", "Time", width - 29, " ");

	if (signo) {
		plotter_show(signo);
		log_show(signo);
	}
}

static void sigwinch_cb(int signo, void *arg)
{
	(void)signo;
	(void)arg;
	redraw(1);
}

static void exit_loop(int signo, void *arg)
{
	(void)arg;
	DEBUG("\nWe got signal! (signo: %d)", signo);
	pev_exit(0);
}

static void deadline_cb(int period, void *arg)
{
	(void)period;
	(void)arg;

	DEBUG("\nDeadline reached, exiting.");
	pev_exit(0);
}

#define PG_UP 1
#define PG_DN 2

static int check_esc(int sd)
{
	int rc = -1;
	char ch;

	if (read(sd, &ch, sizeof(ch)) != 1 || ch != '[')
		return -1;

	if (read(sd, &ch, sizeof(ch)) != 1)
		return -1;

	if (ch == '5')
		rc = PG_UP;
	if (ch == '6')
		rc = PG_DN;

	if (read(sd, &ch, sizeof(ch)) != 1 || ch != '~')
		return -1;

	return rc;
}

static void key_cb(int sd, void *arg)
{
	char ch;

	(void)arg;

	if (read(sd, &ch, sizeof(ch)) != -1) {
		switch (ch) {
		case '\f':
			redraw(1);
			break;

		case '\e':
			switch (check_esc(sd)) {
			case PG_UP:
				log_scroll(-1);
				break;

			case PG_DN:
				log_scroll(+1);
				break;

			default:
				/* unhandled esc sequence */
				break;
			}
			break;

		case 'q':
			pev_exit(0);
			break;

		default:
			DEBUG("Got char 0x%02x", ch);
			break;
		}
	}
}

static void scroll_cb(int period, void *arg)
{
	size_t i;

	(void)period;
	(void)arg;

	plotter_show(0);

	for (i = 0; i < group_num; i++) {
		struct gr *g = &groups[i];

		memmove(g->status, &g->status[1], STATUS_HISTORY - 1);
		g->status[STATUS_POS] = ' ';
	}
}

static void clock_cb(int period, void *arg)
{
	char buf[INET_ADDRSTR_LEN] = "0.0.0.0";
	inet_addr_t addr = { 0 };
	char hostname[80];
	time_t now, up;
	char *str;

	(void)period;
	(void)arg;

	gethostname(hostname, sizeof(hostname));
	ifinfo(iface, &addr, AF_UNSPEC);
	inet_address(&addr, buf, sizeof(buf));

	now = time(NULL);
	up = now - start;

	gotoxy(0, HOSTDATE_ROW);
	fprintf(stderr, "%s (%s@%s)", hostname, buf, iface);

	str = uptime(up);
	gotoxy(width - strlen(str) + 1, TITLE_ROW);
	fputs(str, stderr);

	str = ctime(&now);
	gotoxy(width - strlen(str) + 2, HOSTDATE_ROW);
	fputs(str, stderr);

	log_show(0);
}

static int usage(int code)
{
	if (!iface[0])
		ifdefault(iface, sizeof(iface));

	printf("Usage: %s [-dhjosv] [-c COUNT] [-f MSEC ][-i IFACE] [-l LEVEL] [-p PORT]\n"
	       "              [-r SEC] [-t TTL] [-w SEC]\n"
	       "              [[SOURCE,]GROUP0 .. [SOURCE,]GROUPN | [SOURCE,]GROUP+NUM]\n"
	       "Options:\n"
	       "  -b BYTES    Payload in bytes over IP/UDP header (42 bytes), default: 100\n"
	       "  -c COUNT    Stop sending/receiving after COUNT number of packets (per group)\n"
	       "  -d          Run as daemon in background, output except progress to syslog\n"
	       "  -f MSEC     Frequency, poll/send every MSEC milliseoncds, default: %d\n"
	       "  -h          This help text\n"
	       "  -i IFACE    Interface to use for sending/receiving multicast, default: %s\n"
	       "  -j          Join groups, default unless acting as sender\n"
	       "  -l LEVEL    Set log level; none, notice*, debug\n"
	       "  -o          Old (plain/ordinary) output, no fancy progress bars\n"
	       "  -p PORT     UDP port number to send/listen to, default: %d\n"
	       "  -s          Act as sender, sends packets to select groups, default: no\n"
	       "  -t TTL      TTL to use when sending multicast packets, default: 1\n"
	       "  -v          Display program version\n"
	       "  -w SEC      Initial wait before opening sockets\n"
	       "  -W SEC      Timeout, in seconds, before %s exits\n"
	       "\n"
	       "Bug report address : %-40s\n", ident, period / 1000, iface,
	       DEFAULT_PORT, ident, PACKAGE_BUGREPORT);
#ifdef PACKAGE_URL
	printf("Project homepage   : %s\n", PACKAGE_URL);
#endif

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
	struct rlimit rlim;
	int deadline = 0;
	int wait = 0;
	int i, c, rc;
	size_t ilen;

	for (i = 0; i < MAX_NUM_GROUPS; i++)
		memset(&groups[i], 0, sizeof(groups[0]));

	ident = progname(argv[0]);
	while ((c = getopt(argc, argv, "b:c:df:hi:jl:op:st:vw:W:")) != EOF) {
		switch (c) {
		case 'b':
			bytes = (size_t)atoi(optarg);
			if (bytes > BUFSZ) {
				ERROR("Too long payload, max %d bytes", BUFSZ);
				return 1;
			}
			break;

		case 'c':
			count = (size_t)atoi(optarg);
			break;

		case 'd':
			foreground = 0;
			break;

		case 'f':
			period = atoi(optarg) * 1000;
			break;

		case 'h':
			return usage(0);

		case 'i':
			ilen = strlen(optarg);
			if (ilen >= sizeof(iface)) {
				ERROR("Too long interface name, max %zd chars.", sizeof(iface) - 1);
				return 1;
			}
			strncpy(iface, optarg, sizeof(iface));
			iface[ilen] = 0;
			DEBUG("IFACE: %s", iface);
			break;

		case 'j':
			join++;
			break;

		case 'l':
			if (log_level(optarg)) {
				ERROR("Invalid log level: %s", optarg);
				return 1;
			}
			break;

		case 'o':
			old++;
			break;

		case 'p':
			port = atoi(optarg);
			if (port < 1024 && geteuid())
				ERROR("Must be root to use privileged ports (< 1024)");
			break;

		case 's':
			join = 0;
			break;

		case 't':
			ttl = atoi(optarg);
			break;

		case 'v':
			printf("%s\n", PACKAGE_VERSION);
			return 0;

		case 'w':
			wait = atoi(optarg);
			break;

		case 'W':
			deadline = atoi(optarg);
			break;

		default:
			return usage(1);
		}
	}

	if (optind == argc)
		groups[group_num++].group = strdup(DEFAULT_GROUP);

	if (!foreground) {
		if (daemonize()) {
			printf("Failed backgrounding: %s", strerror(errno));
			_exit(1);
		}
	} else if (!old) {
		if (isatty(STDOUT_FILENO)) {
			setvbuf(stdout, NULL, _IONBF, 0);
			ttsize(&width, &height);
		} else {
			hastty = 0;
			old = 1;
		}
	}

	if (wait)
		sleep(wait);

	log_init(foreground, ident);

	if (!iface[0])
		ifdefault(iface, sizeof(iface));

	if (getrlimit(RLIMIT_NOFILE, &rlim)) {
		ERROR("Failed reading RLIMIT_NOFILE");
		return 1;
	}

	DEBUG("NOFILE: current %ld max %ld", rlim.rlim_cur, rlim.rlim_max);
	rlim.rlim_cur = MAX_NUM_GROUPS + 10; /* Need stdio + pollfd, etc. */
	if (setrlimit(RLIMIT_NOFILE, &rlim)) {
		ERROR("Failed setting RLIMIT_NOFILE soft limit to %d", MAX_NUM_GROUPS);
		return 1;
	}
	DEBUG("NOFILE: set new current %ld max %ld", rlim.rlim_cur, rlim.rlim_max);

	/*
	 * mcjoin group+num
	 * mcjoin group0 group1 group2
	 */
	for (i = optind; i < argc; i++) {
		char *pos, *group, *source = NULL;
		char buf[INET_ADDRSTR_LEN + 1];
		int j, num = 1;

		strlcpy(buf, argv[i], sizeof(buf));
		pos = strchr(buf, '+');
		if (pos) {
			*pos = 0;
			num = atoi(&pos[1]);
		}
		group = buf;

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
#ifdef AF_INET6
			struct sockaddr_in6 *sin6;
#endif
			struct sockaddr_in *sin;
			inet_addr_t addr;
			socklen_t len;
			uint32_t step;
			void *ptr;
			int family;

#ifdef AF_INET6
			if (strchr(group, ':')) {
				family = AF_INET6;
				sin6 = (struct sockaddr_in6 *)&addr;
				ptr = &sin6->sin6_addr;
			} else
#endif
			{
				family = AF_INET;
				sin = (struct sockaddr_in *)&addr;
				ptr = &sin->sin_addr;
			}

			DEBUG("Converting family %d group %s to ptr %p, num :%d ... ", family, group, ptr, num);
			if (!inet_pton(family, group, ptr)) {
				ERROR("%s is not a valid multicast group", group);
				return usage(1);
			}

			DEBUG("Adding (S,G) %s,%s to list ...", source ?: "*", group);
			groups[group_num].source  = source;
			groups[group_num++].group = strdup(group);

			/* Next group ... */
#ifdef AF_INET6
			if (strchr(group, ':')) {
				memcpy(&step, &sin6->sin6_addr.s6_addr[12],
				    sizeof(step));
				step = htonl(ntohl(step) + 1);
				memcpy(&sin6->sin6_addr.s6_addr[12], &step,
				    sizeof(step));
				len = sizeof(*sin6);
			} else
#endif
			{
				step = ntohl(sin->sin_addr.s_addr);
				step++;
				sin->sin_addr.s_addr = htonl(step);
				len = sizeof(*sin);
			}

			inet_ntop(family, ptr, buf, len);
			group = buf;
		}
	}

	for (i = 0; i < (int)group_num; i++) {
#ifdef AF_INET6
		if (strchr(groups[i].group, ':')) {
			struct sockaddr_in6 *grp = (struct sockaddr_in6 *)&groups[i].grp;
			struct sockaddr_in6 *src = (struct sockaddr_in6 *)&groups[i].src;

			inet_pton(AF_INET6, groups[i].group, &grp->sin6_addr);
			grp->sin6_family = AF_INET6;
			grp->sin6_port   = htons(port);

			if (groups[i].source) {
				inet_pton(AF_INET6, groups[i].source, &src->sin6_addr);
				src->sin6_family = AF_INET6;
				src->sin6_port   = 0;
			}
			need6++;
		} else
#endif
		{
			struct sockaddr_in *grp = (struct sockaddr_in *)&groups[i].grp;
			struct sockaddr_in *src = (struct sockaddr_in *)&groups[i].src;

			inet_pton(AF_INET, groups[i].group, &grp->sin_addr);
			grp->sin_family = AF_INET;
			grp->sin_port   = htons(port);

			if (groups[i].source) {
				inet_pton(AF_INET, groups[i].source, &src->sin_addr);
				src->sin_family = AF_INET;
				src->sin_port   = 0;
			}
			need4++;
		}
#ifdef HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN
		groups[i].grp.ss_len = inet_addrlen(&groups[i].grp);
		if (groups[i].source)
			groups[i].src.ss_len = inet_addrlen(&groups[i].src);
#endif

		memset(groups[i].status, ' ', STATUS_HISTORY - 1);
		groups[i].spin  = groups[i].group[strlen(groups[i].group) - 1];
	}

	pev_init();
	pev_sig_add(SIGINT,   exit_loop, NULL);
	pev_sig_add(SIGHUP,   exit_loop, NULL);
	pev_sig_add(SIGTERM,  exit_loop, NULL);
	pev_timer_add(period, scroll_cb, NULL);
	if (!old) {
		int flags;

		pev_sig_add(SIGWINCH, sigwinch_cb, NULL);
		pev_timer_add(1000000, clock_cb, NULL);

		flags = fcntl(STDIN_FILENO, F_GETFL);
		if (flags != -1)
			(void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
		pev_sock_add(STDIN_FILENO, key_cb, NULL);
	}
	if (deadline)
		pev_timer_add(deadline * 1000000, deadline_cb, NULL);

	if (!join) {
		if ((rc = sender_init())) {
			printf("Failed initializing sender, return code %d, aborting.\n", rc);
			return 1;
		}
	} else {
		if ((rc = receiver_init())) {
			printf("Failed initializing receiver, return code %d, aborting.\n", rc);
			return 1;
		}
	}

	if (foreground && !old) {
		start = time(NULL);
		ttraw();
		hidecursor();
	}

	redraw(1);

	rc = pev_run();
	if (!rc) {
		DEBUG("Leaving main loop");
		show_stats();
	}

	if (foreground) {
		if (!old) {
			gotoxy(0, EXIT_ROW);
			showcursor();
			ttcooked();
		} else
			puts("");
	}

	return rc;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
