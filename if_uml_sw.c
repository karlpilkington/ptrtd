/*
 *  if_uml_sw.c
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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/un.h>

#include "config.h"
#include "defs.h"
#include "event.h"
#include "pbuf.h"
#include "if.h"
#include "ether.h"
#include "util.h"

struct iface_uml_sw
{
	struct iface_ether ife;
	struct sockaddr_un swaddr;
	struct sockaddr_un myaddr;
	int cfd;
	int dfd;
};

/* COPIED FROM uml_router.c */

enum request_type
{ REQ_NEW_CONTROL };

struct request
{
	enum request_type type;
	union
	{
		struct
		{
			unsigned char addr[6];
			struct sockaddr_un name;
		} new_control;
		struct
		{
			unsigned long cookie;
		} new_data;
	} u;
};

/* END OF COPIED CODE */

static int
uml_sw_read_callback (struct event *e, void *d)
{
	struct iface_uml_sw *i = (struct iface_uml_sw *) d;
	struct pbuf *pkt;
	int ret;

	pkt = pbuf_new (i->ife.iface.mtu + i->ife.head_size);
	pbuf_drop (pkt, i->ife.head_size - 14);
	ret = recv (e->ev.fd.fd, pkt->d, pkt->max, 0);
	if (ret <= 0) {
		fprintf (stderr, "socket read returned %d\n", ret);
		if (ret < 0)
			perror ("read from socket");
		exit (1);
	}
	fprintf (stderr, "Read from uml_switch\n");
	pkt->dlen = ret;
	i->ife.recv_frame (&i->ife, pkt);
	pbuf_delete (pkt);
	return 1;
}

static int
uml_sw_send_frame (struct iface_ether *iface, struct pbuf *p)
{
	struct iface_uml_sw *i = (struct iface_uml_sw *) iface;

	fprintf (stderr, "Sending to uml_switch\n");

	return sendto (i->dfd, p->d, p->dlen, 0, (struct sockaddr *) &i->swaddr, sizeof (struct sockaddr_un)) < 0 ? -1 : 0;
}

static struct iface *
uml_sw_new_if (char *arg, void (*handle_pkt) (struct iface * iface, struct pbuf * pkt))
{
	struct iface_uml_sw *i;
	struct sockaddr_un addr;
	char *data_path = "/tmp/uml.data";
	char *ctl_path = "/tmp/uml.ctl";
	struct request req;

	i = ALLOC (sizeof (struct iface_uml_sw));
	memset (i, 0, sizeof (struct iface_uml_sw));

	i->swaddr.sun_family = AF_UNIX;
	strcpy (i->swaddr.sun_path, data_path);
	i->myaddr.sun_family = AF_UNIX;
	sprintf (i->myaddr.sun_path, "/tmp/uml_sw.%d", getpid ());
	printf ("binding to socket %s\n", i->myaddr.sun_path);

	ether_setup ((struct iface_ether *) i);
	i->ife.pkt_handler = handle_pkt;
	i->ife.send_frame = uml_sw_send_frame;
	i->ife.iface.mtu = 1280;

	if ((i->dfd = socket (PF_UNIX, SOCK_DGRAM, 0)) < 0) {
		perror ("uml_sw data socket");
		exit (1);
	}
	if (bind (i->dfd, (struct sockaddr *) &i->myaddr, sizeof (struct sockaddr_un)) < 0) {
		perror ("uml_sw data socket bind");
		exit (1);
	}

	addr.sun_family = AF_UNIX;
	strcpy (addr.sun_path, ctl_path);
	if ((i->cfd = socket (PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror ("uml_sw control socket");
		exit (1);
	}
	if (connect (i->cfd, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
		perror ("uml_sw connect control socket");
		exit (1);
	}
	i->ife.hwaddr[0] = 0x02;
	i->ife.hwaddr[1] = 0;
	PUT_32 (i->ife.hwaddr + 2, getpid ());

	req.type = REQ_NEW_CONTROL;
	memcpy (&req.u.new_control.name, &i->myaddr, sizeof (i->myaddr));
	memcpy (req.u.new_control.addr, i->ife.hwaddr, 6);
	if (write (i->cfd, &req, sizeof (req)) < 0) {
		perror ("send request to control socket");
		exit (1);
	}

	add_fd_event (i->dfd, 0, uml_sw_read_callback, i);

	return (struct iface *) i;
}

int
uml_sw_register (void)
{
	register_iface_driver ("uml_sw", uml_sw_new_if);
	return 0;
}
