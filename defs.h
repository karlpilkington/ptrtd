/*
 *  defs.h
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


#ifndef _DEFS_H
#define _DEFS_H

#define PUT_16(p,v) ((p)[0]=((v)>>8)&0xff,(p)[1]=(v)&0xff)
#define PUT_32(p,v) ((p)[0]=((v)>>24)&0xff,(p)[1]=((v)>>16)&0xff,(p)[2]=((v)>>8)&0xff,(p)[3]=(v)&0xff)
#define GET_16(p) (((p)[0]<<8)|(p)[1])
#define GET_32(p) (((p)[0]<<24)|((p)[1]<<16)|((p)[2]<<8)|(p)[3])

typedef unsigned char uchar;
//typedef       unsigned int            uint;

#define TCP_CLOSED		0
#define TCP_LISTEN		1
#define	TCP_SYN_SENT		2
#define TCP_SYN_RECVD		3
#define TCP_ESTABLISHED		4
#define TCP_FIN_WAIT_1		5
#define TCP_FIN_WAIT_2		6
#define TCP_CLOSE_WAIT		7
#define TCP_CLOSING		8
#define TCP_LAST_ACK		9
#define TCP_TIME_WAIT		10

struct globals
{
	int debug;
	unsigned short prefix[8];
	int plen;
	char const *config_file;
	unsigned short http_port;
};

extern struct globals globals;

#endif /* _DEFS_H */
