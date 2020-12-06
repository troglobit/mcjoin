/* Join multicast group(s) and display receive progress
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

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "mcjoin.h"

int num_joins = 0;

static int alloc_socket(inet_addr_t group)
{
	inet_addr_t ina = { 0 };
	int sd, val, proto;

	ina.ss_family = group.ss_family;
	inet_addr_set_port(&ina, inet_addr_get_port(&group));

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
			ERROR("Failed disabling IP_MULTICAST_ALL: %s", strerror(errno));
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
		ERROR("Failed binding to socket: %s", strerror(errno));
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
	int sd, op, proto;

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

/*
 * rcvmsg() wrapper which uses out-of-band info to verify expected
 * destination address (multicast group)
 */
static ssize_t recv_mcast(int id)
{
	struct sockaddr_storage src;
	struct in_addr *dstaddr;
	struct in6_addr *dstaddr6;
	struct msghdr msgh;
	struct iovec iov[1];
	char addr[INET6_ADDRSTRLEN];
	char cmbuf[0x100];
	char buf[BUFSZ];
	const char *dst;
	size_t seq = 0;
	ssize_t bytes;
	int pid = 0;
	char *ptr;

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

	dstaddr = find_dstaddr(&msgh);
	if (dstaddr)
		dst = inet_ntop(AF_INET, dstaddr, addr, sizeof(addr));
#ifdef AF_INET6
	else {
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
	if (ptr)
		seq = atoi(ptr + strlen(SEQ_KEY));

	DEBUG("Count %5zu, our PID %d, sender PID %d, group %s, seq: %zu, msg: %s",
	      groups[id].count, getpid(), pid, groups[id].group, seq, buf);

	if (strcmp(dst, groups[id].group)) {
		ERROR("Packet for group %s received on wrong socket, expected group %s.",
		      dst, groups[id].group);
		return -1;
	}

	if (groups[id].seq != seq) {
		DEBUG("group seq %zu vs seq %zu", groups[id].seq, seq);
		groups[id].gaps++;
	}
	groups[id].seq = seq + 1; /* Next expected sequence number */
	groups[id].count++;
	groups[id].status[STATUS_POS] = '.'; /* XXX: Use increasing dot size for more hits? */

	return 0;
}

static void receive_cb(int sd, void *arg)
{
	size_t i;

	(void)arg;

	for (i = 0; i < group_num; i++) {
		if (groups[i].sd == sd) {
			recv_mcast(i);
			plotter_show(0);
			break;
		}
	}

	if (count > 0) {
		size_t total = count * group_num;

		for (i = 0; i < group_num; i++)
			total -= groups[i].count;

		if (total <= 0)
			pev_exit();
	}
}

int receiver_init(void)
{
	size_t i;

	for (i = 0; i < group_num; i++) {
		if (join_group(&groups[i]))
			return 1;

		if (pev_sock_add(groups[i].sd, receive_cb, NULL) == -1)
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
