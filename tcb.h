/*
 *  tcb.h
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

#ifndef _TCB_H
#define _TCB_H

#include <stdio.h>
#include "defs.h"
#include "event.h"
#include "tcp.h"
#include "buffer.h"
#include "rbtree.h"

#ifdef DEBUG
extern int tcb_find_visits;
extern int tcb_find_calls;
#endif

struct tcb
{
	struct rb_node node;

	int state;

	uchar laddr[16];
	int lport;
	uchar raddr[16];
	int rport;

	struct ringbuf *inbuf;
	struct ringbuf *outbuf;

	void *app_data;
	struct tcp_callback *cb;

	uint snd_una;
	uint snd_nxt;
	uint snd_wnd;
	uint snd_cwnd;
	uint snd_max;
	uint iss;
	uint mss;
	uint rcv_nxt;
	uint rcv_wnd;
	uint rcv_up;
	uint irs;

	uint last_acked;

	uint timeout_mark;	// seqnum

	uint rtt_mark;		// seqnum being used to measure RTT
	uint rtt_limit;		// smallest seqnum we can measure RTT with
	time_ref rtt_time;	// when we sent the segment containing rtt_mark
	int srtt;		// smoothed mean RT time, in msec
	int sdev;		// smoothed RT time mean deviation, in msec

	struct timeval start_time;
	struct timeval atime;
	uint packets;
	FILE *fp;

	uint read_seq;		/* next seqnum for app to read */

	struct event *e_tcb_send;
	struct event *e_timeout;
};

extern struct rb_root tcblist;

struct tcb *tcb_new (uchar * laddr, int lport, uchar * raddr, int rport);
void tcb_delete (struct tcb *t);
struct tcb *tcb_find (uchar * laddr, int lport, uchar * raddr, int rport);

#endif /* _TCB_H */
