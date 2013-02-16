/*
 *  udp.c
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
#include "if.h"
#include "icmp.h"
#include "buffer.h"
#include "udp.h"

struct udp_socket
{
	struct udp_socket *next;
	struct udp_socket *prev;
	uchar laddr[16];
	int lport;
	void *app_data;
	struct udp_callback *cb;
};

static struct udp_socket *udp_list = NULL;

extern struct iface *iface;

static void
udp_remove_sock (struct udp_socket *us)
{
	if (us->next)
		us->next->prev = us->prev;
	if (us->prev)
		us->prev->next = us->next;
	if (udp_list == us)
		udp_list = us->next;
	FREE (us);
}

int
handle_udp (uchar * p, int len)
{
	struct udp_socket *us;

	// real lookup goes here
	us = udp_list;

	if (!us)
		return 1;	// return "connection rejected"?

	us->cb->incoming_packet (us->app_data, p + 48, len - 48, p + 24, GET_16 (p + 42), p + 8, GET_16 (p + 40));
	return 1;
}

struct udp_socket *
udp_open (struct udp_callback *cb, void *app_data, uchar * laddr, int lport)
{
	struct udp_socket *us;

	us = ALLOC (sizeof (struct udp_socket));
	us->next = udp_list;
	us->prev = NULL;
	if (udp_list)
		udp_list->prev = us;
	udp_list = us;
	if (laddr)
		memcpy (us->laddr, laddr, 16);
	else
		memset (us->laddr, 0, 16);
	us->lport = lport;
	us->cb = cb;
	us->app_data = app_data;
	return us;
}

int
udp_send (struct udp_socket *us, uchar * data, int len, uchar * laddr, int lport, uchar * raddr, int rport)
{
	int sum;
	struct pbuf *p;

	p = iface->get_buffer (iface, 1280);
	if (len > 1280 - 48)
		len = 1280 - 48;
	memcpy (p->d + 48, data, len);

	memset (p->d, 0, 48);
	p->d[0] = 0x60;		//version
	PUT_16 (p->d + 4, len + 8);
	p->d[6] = 17;		//udp
	p->d[7] = 0x40;		//ttl
	memcpy (p->d + 8, laddr, 16);
	memcpy (p->d + 24, raddr, 16);
	PUT_16 (p->d + 40, lport);
	PUT_16 (p->d + 42, rport);
	PUT_16 (p->d + 44, len + 8);

	sum = ~make_cksum (p->d, len + 48);
	PUT_16 (p->d + 46, sum);

	p->dlen = len + 48;
	send_pkt (iface, p);

	return len;
}

int
udp_close (struct udp_socket *us)
{
	udp_remove_sock (us);
	return 1;
}

uchar *
udp_get_laddr (struct udp_socket * us)
{
	return us->laddr;
}

int
udp_get_lport (struct udp_socket *us)
{
	return us->lport;
}
