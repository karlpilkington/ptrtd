/*
 *  buffer.c
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
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "config.h"
#include "defs.h"
#include "buffer.h"
#include "util.h"

struct ringbuf *
rb_new (int s)
{
	struct ringbuf *r;

	r = ALLOC (sizeof (struct ringbuf));
	r->size = s;
	r->p = NULL;
	r->w_pos = 0;
	r->w_seq = 0;
	r->used = 0;
	return r;
}

void
rb_delete (struct ringbuf *r)
{
	if (r->p)
		FREE (r->p);
	FREE (r);
}

void
rb_set (struct ringbuf *r, uint s)
{
	r->w_seq = s;
}

void
rb_advance (struct ringbuf *r, uint s)
{
	r->used = r->w_seq - s;
	if (r->used <= 0) {
		r->used = 0;
		r->w_seq = s;
	}
	if (r->used == 0) {
		if (r->p) {
			FREE (r->p);
		}
		r->p = NULL;
		r->w_pos = 0;
		syslog (LOG_INFO, "Removing unused buffer.\n");
	}
}

int
rb_left (struct ringbuf const *r)
{
	return r->size - r->used;
}

int
rb_used (struct ringbuf const *r)
{
	return r->used;
}

int
rb_avail (struct ringbuf const *r, uint seq)
{
	int d = r->w_seq - seq;

	if (d > r->used) {
		syslog (LOG_INFO, "wanted to read from %x but only have back to %x!\n", seq, r->w_seq - r->used);
		return 0;
	}
	else
		return d;
}

int
rb_write (struct ringbuf *r, uchar * data, int len)
{
	int done = 0, cnt;

	if (len == 0)
		return 0;

	if (r->p == NULL) {
		r->p = ALLOC (r->size);
		r->w_pos = 0;
		r->used = 0;
	}

	if (rb_left (r) < len)
		len = rb_left (r);

	cnt = r->size - r->w_pos;
	if (len < cnt)
		cnt = len;

	memcpy (r->p + r->w_pos, data + done, cnt);
	len -= cnt;
	done += cnt;
	r->used += cnt;
	r->w_pos += cnt;

	if (r->w_pos == r->size) {
		if (len > 0) {
			memcpy (r->p, data + done, len);
			done += len;
			r->used += len;
			r->w_pos = len;
		}
		else
			r->w_pos = 0;
	}

	r->w_seq += done;

	return done;
}

int
rb_read (struct ringbuf *r, uint seq, uchar * data, int max)
{
	int back, pos, cnt, done = 0;

	back = r->w_seq - seq;
	if (back == 0 || r->used < back || max == 0)
		return 0;

	if (back < max)
		max = back;

	if (back > r->w_pos) {
		cnt = back - r->w_pos;
		pos = r->size - cnt;
		if (cnt > max)
			cnt = max;
		memcpy (data + done, r->p + pos, cnt);
		max -= cnt;
		done += cnt;
		back -= cnt;
	}

	if (max > 0) {
		memcpy (data + done, r->p + r->w_pos - back, max);
		done += max;
	}

	return done;
}
