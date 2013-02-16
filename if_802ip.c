/*
 *  802ip.c
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

/* default: 239.192.168.1:1102 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.h"
#include "config.h"
#include "defs.h"
#include "event.h"
#include "pbuf.h"
#include "if.h"
#include "ether.h"

struct iface_802ip
{
	struct iface_ether ife;
	struct sockaddr_in mcast_addr;
	int mfd;
	int ufd;
};

static int
_802ip_read_callback (struct event *e, void *d)
{
	struct iface_802ip *i = (struct iface_802ip *) d;
	struct pbuf *pkt;
	struct sockaddr_in src;
	int ret, socklen;

	pkt = pbuf_new (i->ife.iface.mtu + i->ife.head_size);
	pbuf_drop (pkt, i->ife.head_size - 14);
	socklen = sizeof (src);
	ret = recvfrom (e->ev.fd.fd, pkt->d, pkt->max, 0, (struct sockaddr *) &src, &socklen);
	if (ret <= 0) {
		fprintf (stderr, "socket read returned %d\n", ret);
		if (ret < 0)
			perror ("read from socket");
		exit (1);
	}
	fprintf (stderr, "Read from %s:%d\n", inet_ntoa (src.sin_addr), ntohs (src.sin_port));
	pkt->dlen = ret;
	i->ife.recv_frame (&i->ife, pkt);
	pbuf_delete (pkt);
	return 1;
}

static int
_802ip_send_frame (struct iface_ether *iface, struct pbuf *p)
{
	struct iface_802ip *i = (struct iface_802ip *) iface;
	struct sockaddr_in addr, *s;

	if (1 || p->d[0] & 1) {
		s = &i->mcast_addr;
	}
	else {
		addr.sin_family = AF_INET;
		addr.sin_port = htons (0x6400 | p->d[1]);
		memcpy (&addr.sin_addr.s_addr, p->d + 2, 4);
		s = &addr;
	}
	fprintf (stderr, "Sending to %s:%d\n", inet_ntoa (s->sin_addr), ntohs (s->sin_port));

	return sendto (i->ufd, p->d, p->dlen, 0, (struct sockaddr *) s, sizeof (struct sockaddr_in)) < 0 ? -1 : 0;
}

static struct iface *
_802ip_new_if (char *arg, void (*handle_pkt) (struct iface * iface, struct pbuf * pkt))
{
	struct iface_802ip *i;
	struct sockaddr_in addr;
	struct ip_mreq mr;
	int soarg, port;

	i = ALLOC (sizeof (struct iface_802ip));
	memset (i, 0, sizeof (struct iface_802ip));

	ether_setup ((struct iface_ether *) i);
	i->ife.pkt_handler = handle_pkt;
	i->ife.send_frame = _802ip_send_frame;
	i->ife.iface.mtu = 1280;

	/* default: 239.192.168.1:1102 */
	i->mcast_addr.sin_family = AF_INET;
	i->mcast_addr.sin_port = htons (1102);
	inet_aton ("239.192.168.1", &i->mcast_addr.sin_addr);

	if ((i->mfd = socket (AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror ("multicast socket");
		exit (1);
	}
	soarg = 1;
	if (setsockopt (i->mfd, SOL_SOCKET, SO_REUSEADDR, &soarg, sizeof (soarg)) < 0) {
		perror ("multicast setsockopt SO_REUSEADDR");
		exit (1);
	}
	memcpy (&addr, &i->mcast_addr, sizeof (addr));
	addr.sin_addr.s_addr = 0;
	if (bind (i->mfd, (struct sockaddr *) &addr, sizeof (struct sockaddr_in)) < 0) {
		perror ("multicast socket bind");
		exit (1);
	}
	mr.imr_multiaddr.s_addr = i->mcast_addr.sin_addr.s_addr;
	mr.imr_interface.s_addr = 0;
	if (setsockopt (i->mfd, SOL_IP, IP_ADD_MEMBERSHIP, &mr, sizeof (mr)) < 0) {
		perror ("multicast group join");
		exit (1);
	}

	if ((i->ufd = socket (AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror ("multicast source socket");
		exit (1);
	}
	soarg = 0;
	if (setsockopt (i->ufd, SOL_IP, IP_MULTICAST_TTL, &soarg, sizeof (soarg)) < 0) {
		perror ("setsockopt IP_MULTICAST_TTL");
		exit (1);
	}
	soarg = 1;
	if (setsockopt (i->ufd, SOL_IP, IP_MULTICAST_LOOP, &soarg, sizeof (soarg)) < 0) {
		perror ("setsockopt IP_MULTICAST_LOOP");
		exit (1);
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = 0;
	for (port = 0x6400; port < 0x6500; ++port) {
		addr.sin_port = htons (port);
		if (bind (i->ufd, (struct sockaddr *) &addr, sizeof (struct sockaddr_in)) < 0) {
			if (errno != EADDRINUSE) {
				perror ("socket bind");
				exit (1);
			}
		}
		else
			break;
	}
	if (port == 0x6500)	// uh oh, no port...
	{
		fprintf (stderr, "No ports available for 802.IP!\n");
		exit (1);
	}

	i->ife.hwaddr[0] = 0x02;
	i->ife.hwaddr[1] = port & 0xff;
	PUT_32 (i->ife.hwaddr + 2, 0xaaaaaaaa);

	add_fd_event (i->mfd, 0, _802ip_read_callback, i);

	return (struct iface *) i;
}

int
_802ip_register (void)
{
	register_iface_driver ("802ip", _802ip_new_if);
	return 0;
}
