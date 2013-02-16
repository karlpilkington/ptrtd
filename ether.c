/*
 *  ether.c
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

#include "config.h"
#include "defs.h"
#include "event.h"
#include "pbuf.h"
#include "if.h"
#include "ether.h"

static void
ether_get_hwaddr (struct iface *iface, uchar * addr)
{
	memcpy (addr, ((struct iface_ether *) iface)->hwaddr, 6);
}

static struct pbuf *
ether_get_buffer (struct iface *iface, int size)
{
	struct pbuf *p;
	struct iface_ether *i = (struct iface_ether *) iface;

	p = pbuf_new (size + i->head_size);
	pbuf_drop (p, i->head_size);
	return p;
}

static int
ether_send_unicast (struct iface *iface, struct pbuf *p, uchar * hwdest)
{
	struct iface_ether *i = (struct iface_ether *) iface;
	int ret;

	//dump_packet( "Send on link", p );
	pbuf_raise (p, 14);
	memcpy (p->d, hwdest, 6);
	memcpy (p->d + 6, i->hwaddr, 6);
	PUT_16 (p->d + 12, ETHERNET_IPV6_TYPE);

	ret = i->send_frame (i, p);
	pbuf_delete (p);
	return ret;
}

static int
ether_send_multicast (struct iface *iface, struct pbuf *p, uchar * dest)
{
	struct iface_ether *i = (struct iface_ether *) iface;
	int ret;

	pbuf_raise (p, 14);
	p->d[0] = p->d[1] = 0x33;
	memcpy (p->d + 2, dest + 12, 4);
	memcpy (p->d + 6, i->hwaddr, 6);
	PUT_16 (p->d + 12, ETHERNET_IPV6_TYPE);

	ret = i->send_frame (i, p);
	pbuf_delete (p);
	return ret;
}

int
ether_pkt_in (struct iface_ether *iface, struct pbuf *pkt)
{
	if (GET_16 (pkt->d + 12) == ETHERNET_IPV6_TYPE) {
		memcmp (iface->hwaddr, pkt->d + 6, 6);
		pbuf_drop (pkt, 14);
		//dump_packet( "Receive on link", pkt );
		(*iface->pkt_handler) ((struct iface *) iface, pkt);
	}
	return 0;
}

int
ether_setup (struct iface_ether *iface)
{
	iface->iface.mtu = 1500;
	iface->iface.hwaddr_len = 6;
	iface->iface.get_buffer = ether_get_buffer;
	iface->iface.get_hwaddr = ether_get_hwaddr;
	iface->iface.send_unicast = ether_send_unicast;
	iface->iface.send_multicast = ether_send_multicast;
	iface->recv_frame = ether_pkt_in;
	iface->head_size = 14;
	return 0;
}
