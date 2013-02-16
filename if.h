/*
 *  if.h
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

#ifndef _IF_H
#define _IF_H

#include "event.h"
#include "pbuf.h"

struct iface
{
	int hwaddr_len;
	int mtu;
	void (*get_hwaddr) (struct iface * iface, uchar * addr);
	struct pbuf *(*get_buffer) (struct iface * iface, int size);
	int (*send_unicast) (struct iface * iface, struct pbuf * pb, uchar * hwdest);
	int (*send_multicast) (struct iface * iface, struct pbuf * pb, uchar * dest);
};

int register_iface_driver (char *name, struct iface *(*create_func) (char *arg, void (*handle_pkt)




								       (struct iface * iface, struct pbuf * pkt)));
struct iface *create_iface (char *name, char *arg, void (*handle_pkt) (struct iface * iface, struct pbuf * pkt));
void init_iface_drivers (void);

void tap_get_name (struct iface *iface, char *name);
void tun_get_name (struct iface *iface, char *name);

#endif /* _IF_H */
