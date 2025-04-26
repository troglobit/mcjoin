/* Join multicast group(s) and display receive progress
 *
 * Copyright (C) 2004       David Stevens <dlstevens()us!ibm!com>
 * Copyright (C) 2008-2021  Joachim Wiberg <troglobit()gmail!com>
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

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "mcjoin.h"


static int alloc_socket(inet_addr_t group)
{
	inet_addr_t ina = { 0 };
	int sd, val, proto;
	int port;

	ina.ss_family = group.ss_family;
	port = inet_addr_get_port(&group);
	inet_addr_set_port(&ina, port);

#ifdef AF_INET6
	if (group.ss_family == AF_INET6)
		proto = IPPROTO_IPV6;
	else
#endif
		proto = IPPROTO_IP;

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

#ifdef AF_INET6
	if (proto == IPPROTO_IPV6) {
		if (setsockopt(sd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val, sizeof(val)))
			ERROR("Failed enabling IPV6_RECVPKTINFO: %s", strerror(errno));
#ifdef IPV6_MULTICAST_ALL
		val = 0;
		if (setsockopt(sd, proto, IPV6_MULTICAST_ALL, &val, sizeof(val)))
			ERROR("Failed disabling IPV6_MULTICAST_ALL: %s", strerror(errno));
#endif
	}
#endif
	else {
		val = 1;
#if defined(IP_PKTINFO) || !defined(IP_RECVDSTADDR)
		if (setsockopt(sd, SOL_IP, IP_PKTINFO, &val, sizeof(val)))
			ERROR("Failed enabling IP_PKTINFO: %s", strerror(errno));
#elif defined(IP_RECVDSTADDR)
		if (setsockopt(sd, proto, IP_RECVDSTADDR, &val, sizeof(val)))
			ERROR("Failed enabling IP_RECVDSTADDR: %s", strerror(errno));
#endif
#ifdef IP_MULTICAST_ALL
		val = 0;
		if (setsockopt(sd, proto, IP_MULTICAST_ALL, &val, sizeof(val)))
			ERROR("Failed disabling IP_MULTICAST_ALL: %s", strerror(errno));
#endif
	}

	if (bind(sd, (struct sockaddr *)&ina, inet_addrlen(&ina))) {
		switch (errno) {
		case EPERM:
		case EACCES:
			if (ntohs(port) < 1024)
				FATAL("Not root, or no capabilities, binding to privileged port %d.", ntohs(port));
			/* fallthrough */
		default:
			ERROR("Failed binding to socket: %s", strerror(errno));
			break;
		}
		close(sd);
		return -1;
	}

	return sd;
}

static int join_group(struct gr *sg)
{
	char src[INET_ADDRSTR_LEN] = "*";
	char grp[INET_ADDRSTR_LEN] = "";
	struct group_source_req gsr;
	struct group_req gr;
	int sd, op, proto;
	int ifindex;
	size_t len;
	void *arg;

	ifindex = if_nametoindex(iface);
	if (!ifindex) {
		ERROR("invalid interface: %s", *iface ? iface : "(nil)");
		return 1;
	}
	DEBUG("Added iface %s, ifindex %d", iface, ifindex);

	sd = alloc_socket(sg->grp);
	if (sd < 0) {
		DEBUG("Failed allocating socket.");
		return 1;
	}

#ifdef AF_INET6
	if (sg->grp.ss_family == AF_INET6)
		proto = IPPROTO_IPV6;
	else
#endif
		proto = IPPROTO_IP;

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
		inet_address(&sg->src, src, sizeof(src));
	inet_address(&sg->grp, grp, sizeof(grp));
	PRINT("Joining (%s,%s) on %s, ifindex: %d, sd: %d", src, grp, iface, ifindex, sd);

	if (setsockopt(sd, proto, op, arg, len)) {
		ERROR("Failed %s group (%s,%s) on sd %d ... %d: %s",
		      src, grp, "joining", sd, errno, strerror(errno));
		goto error;
	}
	sg->sd = sd;

	return 0;

error:
	close(sd);
	return 1;
}

struct in_addr *find_dstaddr(struct msghdr *msgh)
{
	struct cmsghdr *cmsg;

	for (cmsg = CMSG_FIRSTHDR(msgh); cmsg; cmsg = CMSG_NXTHDR(msgh, cmsg)) {
#if defined(IP_PKTINFO) || !defined(IP_RECVDSTADDR)
		if (cmsg->cmsg_level == SOL_IP &&
		    cmsg->cmsg_type == IP_PKTINFO)
			return &((struct in_pktinfo *)CMSG_DATA(cmsg))->ipi_addr;
#elif defined(IP_RECVDSTADDR)
		if (cmsg->cmsg_level == IPPROTO_IP &&
		    cmsg->cmsg_type == IP_RECVDSTADDR)
			return (struct in_addr *)CMSG_DATA(cmsg);
#endif
	}

	return NULL;
}

struct in6_addr *find_dstaddr6(struct msghdr *msgh)
{
	struct cmsghdr *cmsg;

	for (cmsg = CMSG_FIRSTHDR(msgh); cmsg; cmsg = CMSG_NXTHDR(msgh, cmsg)) {
#if defined(IPV6_PKTINFO)
		if (cmsg->cmsg_level == IPPROTO_IPV6 &&
		    cmsg->cmsg_type == IPV6_PKTINFO)
			return &((struct in6_pktinfo *)CMSG_DATA(cmsg))->ipi6_addr;
#endif
	}

	return NULL;
}

static int isdup(const struct gr *g, size_t seq)
{
	size_t i;

	for (i = 0; i <= STATUS_POS; i++) {
		if (g->seqnos[i] == 0)
			continue;

		if (g->seqnos[i] == seq)
			return 1;
	}

	return 0;
}

/*
 * rcvmsg() wrapper which uses out-of-band info to verify expected
 * destination address (multicast group)
 */
static ssize_t recv_mcast(int sd, struct gr *g)
{
	struct sockaddr_storage src;
	char addr[INET6_ADDRSTRLEN];
	struct in_addr *dstaddr;
	struct iovec iov[1];
	struct msghdr msgh;
	char cmbuf[0x100];
	char buf[BUFSZ];
	const char *dst;
	const char *ptr;
	size_t seq = 0;
	ssize_t bytes;
	int pid = 0;

	iov[0].iov_base = buf;
	iov[0].iov_len  = sizeof(buf);

	memset(&msgh, 0, sizeof(msgh));
	msgh.msg_name       = &src;
	msgh.msg_namelen    = sizeof(src);
	msgh.msg_iov        = iov;
	msgh.msg_iovlen     = NELEMS(iov);
	msgh.msg_control    = cmbuf;
	msgh.msg_controllen = sizeof(cmbuf);

	bytes = recvmsg(sd, &msgh, MSG_DONTWAIT);
	if (bytes < 0)
		return -1;

	dstaddr = find_dstaddr(&msgh);
	if (dstaddr)
		dst = inet_ntop(AF_INET, dstaddr, addr, sizeof(addr));
#ifdef AF_INET6
	else {
		struct in6_addr *dstaddr6;

		dstaddr6 = find_dstaddr6(&msgh);
		if (!dstaddr6)
			return -1;

		dst = inet_ntop(AF_INET6, dstaddr6, addr, sizeof(addr));
	}
#endif

	buf[bytes] = 0;
	ptr = strstr(buf, MAGIC_KEY);
	if (ptr)
		pid = atoi(ptr + strlen(MAGIC_KEY));
	ptr = strstr(buf, SEQ_KEY);
	if (!ptr) {
		g->invalid++;
		g->status[STATUS_POS] = 'I';
		g->count++;
		return -1;
	}
	seq = atoi(ptr + strlen(SEQ_KEY));

	DEBUG("Count %5zu, our PID %d, sender PID %d, group %s, exp. seq: %zu, recv. seq: %zu, msg: %s",
	      g->count, getpid(), pid, g->group, g->seq, seq, buf);

	if (strcmp(dst, g->group)) {
		ERROR("Packet for group %s received on wrong socket, expected group %s.",
		      dst, g->group);
		return -1;
	}

	if (g->seq > 0 && g->seq != seq) {
		if (seq == 0) {
			size_t i;

			/* sender restarted, clear history to prevent false dup counts */
			for (i = 0; i < STATUS_POS; i++)
				g->seqnos[i] = 0;

			g->gaps++;
			g->status[STATUS_POS] = ' ';
		} else {
			if (isdup(g, seq)) {
				g->dupes++;
				g->status[STATUS_POS] = ':';
			} else if (seq < g->seq) {
				g->order++;
				g->status[STATUS_POS] = '<';
			} else { /* seq > g->seq */
				g->gaps++;
				g->status[STATUS_POS] = ' ';
			}
		}
	} else {
		if (g->status[STATUS_POS - 1] == ' ' && g->seq > 1) {
			g->status[STATUS_POS] = '_';
			g->delayed++;
		} else
			g->status[STATUS_POS] = '.';
	}

	g->seqnos[STATUS_POS] = seq;
	g->seq = seq + 1; /* Next expected sequence number */
	g->bytes += bytes;
	g->count++;

	return 0;
}

static void receive_cb(int sd, void *arg)
{
	struct gr *g;

	(void)arg;

	TAILQ_FOREACH(g, &groups, entry) {
		if (g->sd == sd) {
			recv_mcast(sd, g);
			break;
		}
	}

	if (count > 0) {
		size_t total = count * group_num;

		TAILQ_FOREACH(g, &groups, entry)
			total -= g->count;

		if (total == 0)
			pev_exit(0);
	}
}

int receiver_init(void)
{
	struct gr *g;

	TAILQ_FOREACH(g, &groups, entry) {
		if (join_group(g))
			return 1;

		if (pev_sock_add(g->sd, receive_cb, NULL) == -1)
			return 1;
	}

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
