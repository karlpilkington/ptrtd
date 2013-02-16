/*
 *  util.c
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
#include <stdlib.h>

#include "config.h"
#include "util.h"


#ifdef TRACK_MEMORY
struct heap_mem
{
	size_t s;
	int ln;
	char const *fn;
};
int g_allocated = 0;
#endif


void *
allocator (size_t s, int ln, char const *fn)
{
#ifdef TRACK_MEMORY
	struct heap_mem *p = malloc (s + sizeof (struct heap_mem));
	p->s = s;
	p->ln = ln;
	p->fn = fn;
	g_allocated += p->s;
	p += 1;
#else
	void *p = malloc (s);
#endif
	return p;
}

void
deallocator (void *ptr, int ln, char const *fn)
{
#ifdef TRACK_MEMORY
	struct heap_mem *p = (struct heap_mem *) ptr;
	g_allocated -= p->s;
	p -= 1;
#else
	void *p = (void *) ptr;
#endif
	free (p);
}

int
max_int (int a, int b)
{
	return a > b ? a : b;
}

void
dump_packet (char *title, struct pbuf *pkt)
{
	int j, len = pkt->dlen;
	char b[17];
	uchar *p = pkt->d;
	printf ("Packet: %s\n", title);

	for (j = 0; j < len; ++j) {
		if (j % 16 == 0)
			printf ("%d  ", j);

		printf ("%02x ", p[j]);
		b[j % 16] = isprint (p[j]) ? p[j] : '.';

		if (j % 16 == 15 || j == len - 1) {
			b[j % 16 + 1] = 0;
			if (j == len - 1) {
				j = j % 16;
				while (++j < 16)
					printf ("   ");
				j = len;
			}
			printf (" %s\n", b);
		}
	}
}

int
ip6tostr (char *str, int size, uchar const *addr)
{
	strcpy (str, "Unknown");
	return inet_ntop (AF_INET6, addr, str, size) != NULL;
}

int
strtoip6 (uchar * addr, char const *str)
{
	memset (addr, 0, 16);
	return (int) inet_pton (AF_INET6, str, addr);
}

int
make_cksum (uchar * p, int len)
{
	int i;
	int c = GET_16 (p + 4) + p[6];

	for (i = 0x8; i < len; ++i)
		c += (i % 2) ? p[i] : (p[i] << 8);
	while (c > 0xffff)
		c = (c >> 16) + (c & 0xffff);
	return c;
}
