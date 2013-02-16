/*
 *  pbuf.c
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
#include <stdio.h>
#include <ctype.h>

#include "util.h"
#include "config.h"
#include "defs.h"
#include "pbuf.h"

struct pbuf *
pbuf_new (int size)
{
	struct pbuf *pb;

	pb = ALLOC (sizeof (struct pbuf) + size);
	memset (pb, 0, sizeof (struct pbuf) + size);
	pb->size = pb->max = size;
	pb->d = pb->buf;
	//fprintf( stderr, "pbufs allocated: %d\n", ++num_pbufs );
	return pb;
}

void
pbuf_delete (struct pbuf *pb)
{
	FREE (pb);
	//fprintf( stderr, "pbufs allocated: %d\n", --num_pbufs );
}

int
pbuf_drop (struct pbuf *pb, int len)
{
	if (pb->max < len)
		return -1;
	pb->max -= len;
	pb->offset += len;
	pb->d = pb->buf + pb->offset;
	pb->dlen -= len;
	return pb->max;
}

int
pbuf_raise (struct pbuf *pb, int len)
{
	if (pb->offset < len)
		return -1;
	pb->max += len;
	pb->offset -= len;
	pb->d = pb->buf + pb->offset;
	pb->dlen += len;
	return pb->max;
}
