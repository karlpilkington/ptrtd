/*
 *  buffer.h
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

#ifndef _BUFFER_H
#define _BUFFER_H

struct ringbuf
{
	uchar *p;
	int size;
	uint w_seq;
	int w_pos;
	int used;
};

struct ringbuf *rb_new (int s);
void rb_delete (struct ringbuf *r);
void rb_set (struct ringbuf *r, uint s);
void rb_advance (struct ringbuf *r, uint s);
int rb_left (struct ringbuf const *r);
int rb_used (struct ringbuf const *r);
int rb_avail (struct ringbuf const *r, uint seq);
int rb_write (struct ringbuf *r, uchar * data, int len);
int rb_read (struct ringbuf *r, uint seq, uchar * data, int max);

#endif /* _BUFFER_H */
