/*
 *  ether.h
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

#ifndef _ETHER_H
#define _ETHER_H

#include "event.h"
#include "pbuf.h"
#include "if.h"

#define ETHERNET_IPV6_TYPE	0x86dd

struct iface_ether
{
	struct iface iface;
	uchar hwaddr[6];
	int head_size;
	int (*send_frame) (struct iface_ether * iface, struct pbuf * pb);
	int (*recv_frame) (struct iface_ether * iface, struct pbuf * pb);
	void (*pkt_handler) (struct iface * iface, struct pbuf * pkt);
};

int ether_setup (struct iface_ether *iface);
int ether_pkt_in (struct iface_ether *iface, struct pbuf *pb);

#endif /* _ETHER_H */
