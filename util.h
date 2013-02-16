/*
 *  util.h
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

#ifndef _UTIL_H
#define _UTIL_H

#include <stdio.h>
#include <arpa/inet.h>

#include "defs.h"
#include "pbuf.h"

extern int g_allocated;
#define ALLOC(s) allocator(s, __LINE__, __FILE__)
#define FREE(s) deallocator(s, __LINE__, __FILE__)

void *allocator (size_t size, int ln, char const *fn);
void deallocator (void *p, int ln, char const *fn);

void dump_packet (char *title, struct pbuf *pkt);
int ip6tostr (char *str, int size, uchar const *addr);
int strtoip6 (uchar * addr, char const *str);
int make_cksum (uchar * p, int len);
int max_int (int a, int b);

#endif /* _UTIL_H */
