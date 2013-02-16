/*
 *  ptrtd-udp.c
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
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "config.h"
#include "event.h"
#include "defs.h"
#include "util.h"
#include "udp.h"

struct udp_map
{
	struct udp_map *prev;
	struct udp_map *next;

	struct udp_socket *us;
	int fd;

	uchar laddr[16];
	int lport;
	uchar raddr[16];
	int rport;

	struct event *e_fd_read;
	struct event *e_stale;
};

static struct udp_map *ulist = NULL;
static struct udp_socket *udp_listener;
static struct udp_callback udp_cb;

static int handle_udp_read (struct event *e, void *d);

static int
remove_udp_map (struct event *e, void *d)
{
	struct udp_map *um = (struct udp_map *) d;

	fprintf (stderr, "removing stale udp map %p\n", d);
	if (ulist == um)
		ulist = um->next;
	if (um->next)
		um->next->prev = um->prev;
	if (um->prev)
		um->prev->next = um->next;
	remove_event (um->e_fd_read);
	close (um->fd);
	//udp_close( um->us );
	FREE (um);
	return 0;
}

static struct udp_map *
udp_get_map (uchar * raddr, int rport)
{
	struct udp_map *um;
	time_ref tr;

	for (um = ulist; um; um = um->next)
		if (um->rport == rport && !memcmp (um->raddr, raddr, 16))
			return um;

	um = ALLOC (sizeof (struct udp_map));
	fprintf (stderr, "creating udp map %p\n", um);
	um->next = ulist;
	um->prev = NULL;
	if (ulist)
		ulist->prev = um;
	ulist = um;
	memcpy (um->raddr, raddr, 16);
	um->rport = rport;
	um->us = udp_listener;
	if ((um->fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror ("can't make UDP socket");
		exit (1);
	}
	um->e_fd_read = add_fd_event (um->fd, 0, handle_udp_read, um);
	time_future (&tr, 600000);
	um->e_stale = add_time_event (&tr, remove_udp_map, um);
	return um;
}

static void
incoming (void *d, uchar * p, int len, uchar * laddr, int lport, uchar * raddr, int rport)
{
	struct udp_map *um;
	struct sockaddr_in dst;
	time_ref tr;

	um = udp_get_map (raddr, rport);
	dst.sin_family = AF_INET;
	memcpy (&dst.sin_addr.s_addr, laddr + 12, 4);
	dst.sin_port = htons (lport);
	fprintf (stderr, "Sending UDP packet to %s:%d\n", inet_ntoa (dst.sin_addr), ntohs (dst.sin_port));
	sendto (um->fd, p, len, 0, (struct sockaddr *) &dst, sizeof (dst));
	time_future (&tr, 600000);
	resched_time_event (um->e_stale, &tr);
}

static int
handle_udp_read (struct event *e, void *d)
{
	struct udp_map *um = (struct udp_map *) d;
	int len, socklen;
	struct sockaddr_in src;
	uchar tb[65536];
	uchar laddr[16];
	uchar pfx[16];
	time_ref tr;

	socklen = sizeof (struct sockaddr_in);
	if ((len = recvfrom (um->fd, tb, sizeof (tb), 0, (struct sockaddr *) &src, &socklen)) < 0) {
		perror ("recvfrom");
		exit (1);
	}

	memcpy (laddr, pfx, 12);
	memcpy (laddr + 12, &src.sin_addr.s_addr, 4);

	udp_send (um->us, tb, len, laddr, ntohs (src.sin_port), um->raddr, um->rport);

	time_future (&tr, 600000);
	resched_time_event (um->e_stale, &tr);

	return 1;
}

void
ptrtd_udp_init (void)
{
	udp_cb.incoming_packet = incoming;
	udp_listener = udp_open (&udp_cb, NULL, NULL, 0);
}

ptrtd_udp_finish (void)
{
	udp_close (udp_listener);
}
