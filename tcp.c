/*
 *  tcp.c
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
#include <syslog.h>

#include "config.h"
#include "event.h"
#include "defs.h"
#include "util.h"
#include "if.h"
#include "icmp.h"
#include "buffer.h"
#include "tcb.h"

extern struct iface *iface;

char const *const stname[] = { "CLOSED", "LISTEN", "SYN_SENT", "SYN_RECVD",
	"ESTABLISHED", "FIN_WAIT_1", "FIN_WAIT_2", "CLOSE_WAIT",
	"CLOSING", "LAST_ACK", "TIME_WAIT"
};

static int do_tcb_write (struct event *e, void *d);

static uint
next_isn (void)
{
	static uint isn = 0;

	return (++isn) << 16;
}

static int
make_tcp_hdr (struct tcb *t, uchar * buf, int dlen, int flags, int optwords)
{
	int sum;
	int totlen = 4 * optwords + dlen + 60;

	memset (buf, 0, 60);
	buf[0] = 0x60;		//version
	PUT_16 (buf + 4, totlen - 40);
	buf[6] = 0x6;		//tcp
	buf[7] = 0x40;		//ttl
	memcpy (buf + 8, t->laddr, 16);
	memcpy (buf + 24, t->raddr, 16);
	PUT_16 (buf + 40, t->lport);
	PUT_16 (buf + 42, t->rport);
	PUT_32 (buf + 44, t->snd_nxt);
	PUT_32 (buf + 48, t->rcv_nxt);
	buf[52] = (optwords + 5) << 4;
	buf[53] = flags;
	PUT_16 (buf + 54, t->rcv_wnd);

	sum = ~make_cksum (buf, totlen);
	PUT_16 (buf + 56, sum);

	t->last_acked = t->rcv_nxt;

	return totlen;
}

static void
mark_for_if_write (struct tcb *t)
{
	if (t->e_tcb_send)
		return;
	t->e_tcb_send = add_always_event (do_tcb_write, t);
}

int
tcp_accept (struct tcb *t)
{
	mark_for_if_write (t);
	return 0;
}

uchar *
tcp_get_raddr (struct tcb * t)
{
	return t->raddr;
}

int
tcp_get_rport (struct tcb *t)
{
	return t->rport;
}

uchar *
tcp_get_laddr (struct tcb * t)
{
	return t->laddr;
}

int
tcp_get_lport (struct tcb *t)
{
	return t->lport;
}

static int
tcp_remove (struct event *e, void *d)
{
	struct tcb *t = d;

	syslog (LOG_INFO, "removing tcb %p\n", d);
	if (t->e_tcb_send)
		remove_event (t->e_tcb_send);
	if (t->e_timeout)
		remove_event (t->e_timeout);
	tcb_delete (t);
	return 0;
}

static inline int
window_size (struct tcb *t)
{
	return t->snd_max - t->snd_nxt;
}

static int
window_update (struct tcb *t)
{
	if (t->snd_cwnd == 0)
		t->snd_cwnd = t->mss;
	if (t->snd_cwnd > t->snd_wnd)
		t->snd_cwnd = t->snd_wnd;
	t->snd_max = t->snd_una + t->snd_cwnd;
	if (t->fp)
		fprintf (t->fp, "snd_wnd=%d snd_cwnd=%d snd_max=%x\n", t->snd_wnd, t->snd_cwnd, t->snd_max);
	return window_size (t);
}

static int
tcp_timeout (struct event *e, void *d)
{
	struct tcb *t = d;

	if (t->fp)
		fprintf (t->fp, "*** timeout!!!  resetting to %x\n", t->snd_una);
	t->e_timeout = NULL;
	t->snd_nxt = t->snd_una;
	t->rtt_mark = t->snd_una - 1;	// don't use lost packets to measure RTT
	t->snd_cwnd = 0;	// this gets reset to the minimum in window_update
	window_update (t);
	mark_for_if_write (t);
	return 1;
}

static void
tcp_fabricate_rst (uchar * data)
{
	struct pbuf *p;
	int sum;

	syslog (LOG_INFO, "Sending TCP rst port=%d\n", GET_16 (data + 40));

	p = (*iface->get_buffer) (iface, 60);
	memset (p->d, 0, 60);
	p->d[0] = 0x60;		//version
	PUT_16 (p->d + 4, 20);
	p->d[6] = 0x6;		//tcp
	p->d[7] = 0x40;		//ttl
	memcpy (p->d + 8, data + 24, 16);
	memcpy (p->d + 24, data + 8, 16);
	PUT_16 (p->d + 40, GET_16 (data + 42));
	PUT_16 (p->d + 42, GET_16 (data + 40));
	PUT_32 (p->d + 44, GET_32 (data + 48));
	PUT_32 (p->d + 48, GET_32 (data + 44) + 1);
	p->d[52] = 5 << 4;
	p->d[53] = 0x14;

	sum = ~make_cksum (p->d, 60);
	PUT_16 (p->d + 56, sum);

	p->dlen = 60;
	//dump_packet( "TCP reset", p );
	send_pkt (iface, p);
}

static void
tcp_send_rst (struct tcb *t)
{
	struct pbuf *p;

	if (t->fp)
		fprintf (t->fp, "Sending TCP rst port=%d\n", t->rport);

	p = (*iface->get_buffer) (iface, 60);
	memset (p->d, 0, 60);
	make_tcp_hdr (t, p->d, 0, 0x14, 0);

	p->dlen = 60;
	//dump_packet( "TCP reset", p );
	send_pkt (iface, p);
	++t->packets;
}

static void
tcp_send_syn (struct tcb *t)
{
	struct pbuf *p;
	int len;

	p = (*iface->get_buffer) (iface, 68);
	p->d[60] = 2;
	p->d[61] = 4;
	PUT_16 (p->d + 62, 1216);
	len = make_tcp_hdr (t, p->d, 0, 0x12, 1);
	if (t->fp)
		fprintf (t->fp, "Sending TCP syn port=%d seq=%x ack=%x flags=%x\n", t->rport, t->snd_nxt, t->rcv_nxt, p->d[53]);
	++t->snd_nxt;
	p->dlen = len;
	send_pkt (iface, p);
	++t->packets;
}

int
tcp_close (struct tcb *t, int hard)
{
	if (t->fp)
		fprintf (t->fp, "closing connection...\n");

	if (hard || t->state == TCP_SYN_RECVD) {
		tcp_send_rst (t);
		if (t->e_tcb_send)
			remove_event (t->e_tcb_send);
		if (t->e_timeout)
			remove_event (t->e_timeout);
		tcb_delete (t);
		return 0;
	}

	t->cb = NULL;
	t->rcv_wnd = 1000;	// let it all drain...

	if (t->state == TCP_ESTABLISHED)
		t->state = TCP_FIN_WAIT_1;
	else
		t->state = TCP_LAST_ACK;
	mark_for_if_write (t);
	return 0;
}

static void
tcp_remote_close (struct tcb *t)
{
	if (t->cb)
		t->cb->closing (t->app_data, 0);
	tcp_close (t, 0);
}

static void
set_timeout (struct tcb *t)
{
	time_ref tr;
	int m;

	m = t->srtt + 4 * t->sdev;
	if (m < 500)
		m = 500;
	else if (m > 30000)
		m = 30000;

	time_future (&tr, m);
	t->e_timeout = add_time_event (&tr, tcp_timeout, t);
	t->timeout_mark = t->snd_nxt;
	if (t->fp)
		fprintf (t->fp, "timeout set for %x\n", t->timeout_mark);
}

// return 1 for can do again, 0 for all done
static int
tcp_send_and_ack (struct tcb *t)
{
	struct pbuf *p;
	int len = 0, wnd;
	int flags = 0x10;

	p = (*iface->get_buffer) (iface, 1500);
	wnd = window_size (t);
	if (wnd > 0) {
		len = rb_avail (t->outbuf, t->snd_nxt);
		if (len > 0) {
			if (len > t->mss)
				len = t->mss;
			if (len > wnd)
				len = wnd;
			len = rb_read (t->outbuf, t->snd_nxt, p->d + 60, len);
			if (rb_avail (t->outbuf, t->snd_nxt + len) == 0)
				flags |= 0x8;	// psh
			if (t->fp)
				fprintf (t->fp, "read %d from buffer to send\n", len);
		}
		else
			len = 0;

		// if we're closing and there's room in the window, send
		// the fin
		if (len < wnd && !rb_avail (t->outbuf, t->snd_nxt + len)
		    && (t->state == TCP_FIN_WAIT_1 || t->state == TCP_CLOSING || t->state == TCP_LAST_ACK))
			flags |= 0x1;
	}
	else if (t->fp)
		fprintf (t->fp, "No data to send because wnd==0\n");

	if (len == 0 && !(flags & 0x1) && t->last_acked >= t->rcv_nxt) {
		if (t->fp)
			fprintf (t->fp, "*** tcp_send_and_ack called, but nothing to do! len=%d\n", len);
		pbuf_delete (p);
		return 0;
	}

	if (!t->e_timeout && len > 0)
		set_timeout (t);

	if (len > 0 && t->rtt_mark < t->snd_una && t->snd_nxt >= t->rtt_limit) {
		t->rtt_mark = t->snd_nxt;
		time_now (&t->rtt_time);
		if (t->fp)
			fprintf (t->fp, "Setting RTT timer at %x\n", t->rtt_mark);
	}

	p->dlen = make_tcp_hdr (t, p->d, len, flags, 0);

	if (t->fp)
		fprintf (t->fp,
			 "Sending TCP data port=%d seq=%x ack=%x datalen=%d flags=%x\n", t->rport, t->snd_nxt, t->rcv_nxt, len, p->d[53]);

	t->snd_nxt += len;
	if (flags & 0x1)
		++t->snd_nxt;

	if (t->rtt_limit < t->snd_nxt)
		t->rtt_limit = t->snd_nxt;

	//dump_packet( "Send TCP data", p );
	send_pkt (iface, p);
	++t->packets;

	wnd = window_size (t);
	if (t->fp)
		fprintf (t->fp, "after this send, %u left in window\n", wnd);

	switch (t->state) {
	case TCP_ESTABLISHED:
		// why was this here?
		//t->cb->output_buffer_space( t->app_data,
		//              rb_left( t->outbuf ) );
		return wnd > 0 && rb_avail (t->outbuf, t->snd_nxt) > 0;
	case TCP_FIN_WAIT_1:
	case TCP_CLOSING:
	case TCP_LAST_ACK:
		return wnd > 0 && rb_avail (t->outbuf, t->snd_nxt) > -1;
	}
	return 0;
}

static int
tcp_recv_data (struct tcb *t, uchar * p, int len)
{
	int doff, dlen, flags;

	flags = p[53] & 0x3f;
	doff = 40 + 4 * (p[52] >> 4);
	dlen = len - doff;

	if (dlen > 0) {
		p[len] = 0;	// only necessary for this printf
		if (t->fp)
			fprintf (t->fp, "data: '%s'\n", p + doff);
		t->rcv_nxt += dlen;
		mark_for_if_write (t);	// need ack

		// if no callbacks, just throw away the received data

		if (t->cb) {
			rb_write (t->inbuf, p + doff, dlen);
			t->rcv_wnd = rb_left (t->inbuf);
			t->cb->data_available (t->app_data, rb_avail (t->inbuf, t->read_seq));
		}
	}

	if (flags & 0x1) {
		++t->rcv_nxt;
		mark_for_if_write (t);
		return 1;
	}

	return 0;
}

int
handle_tcp (uchar * p, int len)
{
	int rport, lport, flags;
	struct tcb *t;

	lport = GET_16 (p + 42);
	rport = GET_16 (p + 40);
	flags = p[53] & 0x3f;
	t = tcb_find (p + 24, lport, p + 8, rport);
	if (t == NULL) {
		syslog (LOG_WARNING, "No matching tcb for %d!\n", lport);
		tcp_fabricate_rst (p);
		return 0;
	}
	gettimeofday (&t->atime, NULL);
	if (t->fp)
		fprintf (t->fp,
			 "Received TCP packet port %d seq=%x ack=%x state=%s flags=%x datalen=%d\n",
			 rport, GET_32 (p + 44), GET_32 (p + 48), stname[t->state], flags, len - (40 + 4 * (p[52] >> 4)));
	if (t->state == TCP_LISTEN) {
		struct tcb *lt = t;

		if (flags == 0x2) {
			uint irs;

			irs = GET_32 (p + 44);
			t = tcb_new (p + 24, lport, p + 8, rport);
			t->inbuf = rb_new (16 * 1024);
			t->outbuf = rb_new (16 * 1024);
			t->state = TCP_SYN_RECVD;
			t->rport = rport;
			memcpy (t->raddr, p + 8, 16);
			t->lport = GET_16 (p + 42);
			memcpy (t->laddr, p + 24, 16);
			t->irs = irs;
			t->rcv_wnd = rb_left (t->inbuf);
			t->read_seq = t->rcv_nxt = irs + 1;
			rb_set (t->inbuf, t->rcv_nxt);
			t->snd_nxt = t->snd_una = t->iss = next_isn ();
			t->rtt_mark = t->snd_nxt - 1;
			t->rtt_limit = t->snd_nxt + 1;
			rb_set (t->outbuf, t->snd_nxt + 1);
			t->mss = 1220;
			t->snd_cwnd = 0;
			t->snd_wnd = GET_16 (p + 54);
			window_update (t);
			t->cb = lt->cb;

			// incoming_session() may call tcp_accept directly,
			// and may even tcp_close immediately

			t->cb->incoming_session (t, &t->app_data, t->fp);
		}
		else
			tcp_fabricate_rst (p);
		return 0;
	}

	if (flags & 0x4) {
		if (t->fp)
			fprintf (t->fp, "connection reset!\n");
		if (t->cb)
			t->cb->closing (t->app_data, 1);
		if (t->e_tcb_send)
			remove_event (t->e_tcb_send);
		if (t->e_timeout)
			remove_event (t->e_timeout);
		tcb_delete (t);
		return 0;
	}

	if (flags & 0x10) {
		uint ack;
		ack = GET_32 (p + 48);
		if (ack > t->snd_una) {
			if (t->snd_una <= t->rtt_mark && ack > t->rtt_mark) {
				int diff;

				diff = time_ago (&t->rtt_time);
				if (t->fp)
					fprintf (t->fp, "RTT=%d msec ", diff);
				if (t->srtt > 0)
					t->srtt = (2 * diff + 8 * t->srtt + 5) / 10;
				else
					t->srtt = diff;
				if (t->fp)
					fprintf (t->fp, "SRTT=%d msec ", t->srtt);
				if (diff > t->srtt)
					t->sdev = (75 * t->sdev + 25 * (diff - t->srtt) + 50) / 100;
				else
					t->sdev = (75 * t->sdev + 25 * (t->srtt - diff) + 50) / 100;
				if (t->fp)
					fprintf (t->fp, "DEV=%d msec\n", t->sdev);
			}

			t->snd_una = ack;
			if (ack > t->snd_nxt)
				t->snd_nxt = ack;
			switch (t->state) {
			case TCP_ESTABLISHED:
				rb_advance (t->outbuf, ack);
				t->cb->output_buffer_space (t->app_data, rb_left (t->outbuf));
				// fallthrough
			case TCP_FIN_WAIT_1:
			case TCP_CLOSING:
			case TCP_LAST_ACK:
				if (t->e_timeout && ack >= t->timeout_mark) {
					remove_event (t->e_timeout);
					if (t->snd_una < t->snd_nxt)
						set_timeout (t);
					else
						t->e_timeout = NULL;
					if (t->fp)
						fprintf (t->fp, "reset timeout timer\n");
					t->snd_cwnd += t->mss;
					// don't call window_update here,
					// we need to test if we're blocked
					// and need to start resending, below
				}
				break;
			}
		}
		if (t->snd_una == t->snd_nxt)
			switch (t->state) {
			case TCP_SYN_RECVD:
				t->state = TCP_ESTABLISHED;
				if (t->fp)
					fprintf (t->fp, "pkt connection accepted.\n");
				t->cb->output_buffer_space (t->app_data, rb_left (t->outbuf));
				break;
			case TCP_LAST_ACK:
				if (t->e_tcb_send)
					remove_event (t->e_tcb_send);
				if (t->e_timeout)
					remove_event (t->e_timeout);
				if (t->fp)
					fprintf (t->fp, "pkt connection closed.\n");
				tcb_delete (t);
				return 0;
			case TCP_FIN_WAIT_1:
				if (rb_avail (t->outbuf, t->snd_nxt) == -1)
					t->state = TCP_FIN_WAIT_2;
				break;
			case TCP_CLOSING:
				if (rb_avail (t->outbuf, t->snd_nxt) == -1) {
					time_ref tr;

					t->state = TCP_TIME_WAIT;
					time_future (&tr, 120000);
					t->e_timeout = add_time_event (&tr, tcp_remove, t);
				}
				return 0;
			}
	}

	/* this deals with duplicates and keepalives, but not out-of-order */
	if (GET_32 (p + 44) < t->rcv_nxt) {
		if (t->state != TCP_SYN_RECVD) {
			/* HACK - we need a better way to rexmit acks */
			if (t->fp)
				fprintf (t->fp, "acking duplicate/old packet!\n");
			--t->last_acked;
			mark_for_if_write (t);
		}
		return 0;
	}
	// start sending again if the window goes from zero to nonzero
	t->snd_wnd = GET_16 (p + 54);
	if (window_size (t) <= 0 && window_update (t) > 0) {
		if (t->fp)
			fprintf (t->fp, "window exists now!\n");
		mark_for_if_write (t);
	}
	else
		window_update (t);

	switch (t->state) {
	case TCP_FIN_WAIT_2:
		if (tcp_recv_data (t, p, len)) {
			time_ref tr;

			if (t->fp)
				fprintf (t->fp, "pkt connection closed.\n");
			t->state = TCP_TIME_WAIT;
			time_future (&tr, 120000);
			t->e_timeout = add_time_event (&tr, tcp_remove, t);
		}
		break;
	case TCP_FIN_WAIT_1:
		if (tcp_recv_data (t, p, len)) {
			t->state = TCP_CLOSING;
		}
		break;
	case TCP_ESTABLISHED:
		if (tcp_recv_data (t, p, len)) {
			t->state = TCP_CLOSE_WAIT;

			/* we should really wait for the app to close
			 * the connection before doing this, but
			 * we have no half-open connection support */
			tcp_remote_close (t);
		}
		break;
	}

	return 0;
}

int
tcp_get_output_space (struct tcb *t)
{
	return rb_left (t->outbuf);
}

int
tcp_send (struct tcb *t, uchar * data, int len, int push)
{
	rb_write (t->outbuf, data, len);
	if (window_size (t) > 0)
		mark_for_if_write (t);
	return 0;
}

int
tcp_read (struct tcb *t, uchar * buf, int len)
{
	int blocked;

	// if we were blocked (win = 0) because the buffer was full,
	// we need to make sure we send the peer a window update
	// packet once we're unblocked

	blocked = rb_left (t->inbuf) == 0;

	if (len > rb_avail (t->inbuf, t->read_seq))
		len = rb_avail (t->inbuf, t->read_seq);

	len = rb_read (t->inbuf, t->read_seq, buf, len);

	t->read_seq += len;
	rb_advance (t->inbuf, t->read_seq);
	t->rcv_wnd = rb_left (t->inbuf);

	if (blocked) {
		/* UGLY, UGLY HACK to force a window update */
		--t->last_acked;
		mark_for_if_write (t);
	}

	return len;
}

static int
do_tcb_write (struct event *e, void *d)
{
	struct tcb *t = d;

	syslog (LOG_INFO, "do_tcb_write called for %p\n", d);

	if (t->state == TCP_SYN_RECVD) {
		tcp_send_syn (t);
		t->e_tcb_send = NULL;
	}
	else if (!tcp_send_and_ack (t))
		t->e_tcb_send = NULL;

	return t->e_tcb_send != NULL;
}

struct tcb *
tcp_listen (struct tcp_callback *cb, uchar * laddr, int lport)
{
	struct tcb *t;

	t = tcb_new (laddr, lport, NULL, 0);
	t->cb = cb;
	t->state = TCP_LISTEN;
	return t;
}
