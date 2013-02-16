/*
 *  udp.h
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

#ifndef _UDP_H
#define _UDP_H

struct udp_socket;

struct udp_callback
{
	void (*incoming_packet) (void *d, uchar * data, int len, uchar * laddr, int lport, uchar * raddr, int rport);
};

struct udp_socket *udp_open (struct udp_callback *cb, void *app_data, uchar * laddr, int lport);
int udp_close (struct udp_socket *us);
int udp_send (struct udp_socket *us, uchar * data, int len, uchar * laddr, int lport, uchar * raddr, int rport);
uchar *udp_get_laddr (struct udp_socket *us);
int udp_get_lport (struct udp_socket *us);

void udp_init (void);

#endif /* _UDP_H */
