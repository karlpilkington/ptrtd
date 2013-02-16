/*
 *  tcp.h
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

#ifndef _TCP_H
#define _TCP_H

struct tcb;

struct tcp_callback
{
	void (*incoming_session) (struct tcb * t, void **app_data, FILE * debug);
	void (*output_buffer_space) (void *s, int space);
	void (*data_available) (void *s, int length);
	void (*closing) (void *s, int hard);
};

struct tcb *tcp_listen (struct tcp_callback *cb, uchar * addr, int port);
struct tcb *tcp_connect (struct tcp_callback *cb, uchar * laddr, int lport, uchar * raddr, int rport);
int tcp_accept (struct tcb *t);
int tcp_close (struct tcb *t, int hard);
int tcp_send (struct tcb *t, uchar * data, int len, int push);
int tcp_read (struct tcb *t, uchar * buf, int len);
int tcp_get_output_space (struct tcb *t);
int tcp_set_output_notify_limit (struct tcb *t, int limit);
uchar *tcp_get_raddr (struct tcb *t);
int tcp_get_rport (struct tcb *t);
uchar *tcp_get_laddr (struct tcb *t);
int tcp_get_lport (struct tcb *t);

void tcp_init (void);

extern char const *const stname[];

#endif /* _TCP_H */
