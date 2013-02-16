/*
 *  icmp.c
 *
 *  ptrtd - Portable IPv6 TRT implementation
 *
 *  Copyright (C) 2001  Nathan Lutchansky
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>

#include "config.h"
#include "event.h"
#include "defs.h"
#include "util.h"
#include "if.h"
#include "buffer.h"

#define ND_INCOMPLETE	0
#define ND_REACHABLE	1
#define ND_STALE	2
#define ND_DELAY	3
#define ND_PROBE	4

struct neighbor
{
	struct neighbor *next;
	struct neighbor *prev;
	struct iface *iface;
	struct pbuf *queue;
	int state;
	uchar addr[16];
	uchar linkaddr[6];
};

extern struct iface *iface;
static uchar mylladdr[16] = { 0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5 };
static uchar otherlladdr[16] = { 0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
static uchar allhostsaddr[16] = { 0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

struct neighbor *nlist = NULL;

static void icmp_send_ns (uchar * dest);

static struct neighbor *
find_neighbor (uchar * addr)
{
	struct neighbor *n;

	for (n = nlist; n; n = n->next)
		if (!memcmp (addr, n->addr, 16))
			return n;
	return NULL;
}

static void
print_neighbor_status (struct neighbor *n)
{
	char addr[INET6_ADDRSTRLEN];

	ip6tostr (addr, sizeof (addr), n->addr);
	if (n->state)
		syslog (LOG_NOTICE, "neighbor %s is at %02x:%02x:%02x:%02x:%02x:%02x", addr,
			 n->linkaddr[0], n->linkaddr[1], n->linkaddr[2], n->linkaddr[3], n->linkaddr[4], n->linkaddr[5]);
	else
	    syslog (LOG_WARNING, "neighbor %s is INCOMPLETE", addr);
}

static struct neighbor *
add_neighbor (struct iface *iface, uchar * addr, uchar * lladdr)
{
	struct neighbor *n;

	n = ALLOC (sizeof (struct neighbor));
	n->next = nlist;
	n->prev = NULL;
	n->queue = NULL;
	n->state = (iface->hwaddr_len == 0 || lladdr)
		? ND_REACHABLE : ND_INCOMPLETE;
	if (nlist)
		nlist->prev = n;
	nlist = n;
	memcpy (n->addr, addr, 16);
	if (lladdr)
		memcpy (n->linkaddr, lladdr, iface->hwaddr_len);
	print_neighbor_status (n);
	return n;
}

void
queue_packet (struct neighbor *n, struct pbuf *p)
{
	struct pbuf *q;

	syslog (LOG_INFO, "queueing a packet...");
	if (!n->queue) {
		n->queue = p;
		return;
	}

	for (q = n->queue; q->next; q = q->next);
	q->next = p;
	p->prev = q;
}

void
send_pkt (struct iface *iface, struct pbuf *p)
{
	struct neighbor *n;
	uchar *dest;

	if (p->d[24] == 0xff) {
		(*iface->send_multicast) (iface, p, p->d + 24);
	}
	else if (iface->hwaddr_len > 0) {
		if (p->d[24] == 0xfe && p->d[25] == 0x80)
			dest = p->d + 24;
		else
			dest = otherlladdr;
		n = find_neighbor (dest);
		if (n) {
			if (n->state == ND_REACHABLE)
				(*iface->send_unicast) (iface, p, n->linkaddr);
			else
				queue_packet (n, p);
		}
		else {
			char addr[INET6_ADDRSTRLEN];

			ip6tostr (addr, sizeof (addr), dest);
			syslog (LOG_WARNING, "no address for %s", addr);
			icmp_send_ns (dest);
			n = add_neighbor (iface, dest, NULL);
			queue_packet (n, p);
		}
	}
	else
		(*iface->send_unicast) (iface, p, NULL);
}

static void
got_neighbor (struct iface *iface, uchar * addr, uchar * lladdr)
{
	struct neighbor *n;

	if ((n = find_neighbor (addr))) {
		if (lladdr)
			memcpy (n->linkaddr, lladdr, iface->hwaddr_len);
		n->state = ND_REACHABLE;
		print_neighbor_status (n);
		if (n->queue) {
			struct pbuf *p, *next;;

			syslog (LOG_INFO, "emptying the queue...");
			for (p = n->queue; p; p = next) {
				next = p->next;
				send_pkt (iface, p);
			}
			n->queue = NULL;
		}
	}
	else
		n = add_neighbor (iface, addr, lladdr);
}

static int
make_icmp_hdr (uchar * p, int len, uchar * src, uchar * dst, int type, int code, int value)
{
	len += 48;
	memset (p, 0, 48);
	p[0] = 0x60;		//version
	PUT_16 (p + 4, len - 40);
	p[6] = 0x3a;		//icmp
	p[7] = 0xff;		//ttl
	memcpy (p + 8, src, 16);
	memcpy (p + 24, dst, 16);
	p[40] = type;
	p[41] = code;
	PUT_32 (p + 44, value);
	PUT_16 (p + 42, 0);
	PUT_16 (p + 42, ~make_cksum (p, len));
	return len;
}

void
icmp_send_error (uchar * sp, int len)
{
	int nlen;
	struct pbuf *p;

	nlen = len > 1232 ? 1232 : len;
	p = (*iface->get_buffer) (iface, nlen);
	memcpy (p->d + 48, sp, nlen);
	p->dlen = make_icmp_hdr (p->d, nlen, sp + 24, sp + 8, 4, 1, 40);
	//dump_packet( "ICMP Error", p );
	send_pkt (iface, p);
}

static void
icmp_send_ping_reply (uchar * sp, int plen)
{
	struct pbuf *p;

	syslog (LOG_NOTICE, "Sending ping reply");

	p = (*iface->get_buffer) (iface, plen);
	memcpy (p->d + 48, sp + 48, plen - 48);
	p->dlen = make_icmp_hdr (p->d, plen - 48, sp + 24, sp + 8, 129, 0, GET_32 (sp + 44));
	//dump_packet( "Ping Reply", p );
	send_pkt (iface, p);
}

static void
icmp_send_ns (uchar * dest)
{
	struct pbuf *p;
	int off = 64;
	uchar sna[16] = { 0xff, 0x02, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0x1, 0xff, 0, 0, 0
	};

	syslog (LOG_INFO, "Sending neighbor solicitation");

	p = (*iface->get_buffer) (iface, 128);
	memcpy (p->d + 48, dest, 16);
	if (iface->hwaddr_len > 0) {
		p->d[off] = 0x1;
		(*iface->get_hwaddr) (iface, p->d + off + 2);
		off += 8 * (p->d[off + 1] = (9 + iface->hwaddr_len) / 8);
	}
	memcpy (sna + 13, dest + 13, 3);
	p->dlen = make_icmp_hdr (p->d, off - 48, mylladdr, sna, 135, 0, 0);
	send_pkt (iface, p);
}

static void
icmp_send_na (uchar * dest)
{
	struct pbuf *p;
	int off = 64;
	uint flags = 0x20000000;

	if (dest)
		flags |= 0x40000000;
	else
		dest = allhostsaddr;

	syslog (LOG_INFO, "Sending neighbor advertisement");

	p = (*iface->get_buffer) (iface, 128);
	memcpy (p->d + 48, mylladdr, 16);
	if (iface->hwaddr_len > 0) {
		p->d[off] = 0x2;
		(*iface->get_hwaddr) (iface, p->d + off + 2);
		off += 8 * (p->d[off + 1] = (9 + iface->hwaddr_len) / 8);
	}
	p->dlen = make_icmp_hdr (p->d, off - 48, mylladdr, dest, 136, 0, flags);
	send_pkt (iface, p);
}

int
handle_icmp (uchar * p, int len)
{
	switch (p[0x28]) {
	case 128:

		icmp_send_ping_reply (p, len);
		break;
	case 135:
		if (iface->hwaddr_len > 0 && p[64] == 1)
			got_neighbor (iface, p + 8, p + 66);
		if (!memcmp (mylladdr, p + 48, 16))
			icmp_send_na (p + 8);
		break;
	case 136:
		if (iface->hwaddr_len > 0 && p[64] == 2)
			got_neighbor (iface, p + 8, p + 66);
		else
			got_neighbor (iface, p + 8, NULL);
		break;
	}

	return 0;
}

int
icmp_init_iface (struct iface *iface)
{
	icmp_send_na (NULL);
	return 0;
}
