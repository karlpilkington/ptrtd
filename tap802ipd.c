/*
 *  tap802ipd.c
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
#include <errno.h>

#include "config.h"
#include "event.h"
#include "defs.h"
#include "if.h"
#include "ether.h"
#include "util.h"

struct iface_ether *tap_iface, *ip_iface;
char *me;
int debug = 0;

void
usage (void)
{
	printf ("Usage: %s [-p <prefix>] [-l <prefix length>]\n", me);
	printf ("    defaults to fec0:0:0:ffff:: len 64\n");
	exit (1);
}

int
handle_tap_frame (struct iface_ether *iface, struct pbuf *p)
{
	dump_packet ("Got from tap", p);
	ip_iface->send_frame (ip_iface, p);
	return 0;
}

int
handle_802ip_frame (struct iface_ether *iface, struct pbuf *p)
{
	if (GET_16 (p->d + 12) == ETHERNET_IPV6_TYPE && memcmp (ip_iface->hwaddr, p->d + 6, 6)) {
		dump_packet ("Got from ip", p);
		tap_iface->send_frame (tap_iface, p);
	}
	return 0;
}

void
init_ifs (char *dev, char *vnet)
{
	char cmd[256], ifname[256];

	ip_iface = (struct iface_ether *) create_iface (vnet, NULL, NULL);
	if (!ip_iface) {
		fprintf (stderr, "Unable to create 802.IP interface.\n");
		exit (1);
	}
	ip_iface->recv_frame = handle_802ip_frame;
	printf ("address %02x:%02x:%02x:%02x:%02x:%02x\n",
		ip_iface->hwaddr[0], ip_iface->hwaddr[1],
		ip_iface->hwaddr[2], ip_iface->hwaddr[3], ip_iface->hwaddr[4], ip_iface->hwaddr[5]);
	tap_iface = (struct iface_ether *) create_iface ("tap", dev, NULL);
	if (!tap_iface) {
		fprintf (stderr, "Unable to create ethertap interface.\n");
		exit (1);
	}
	tap_iface->recv_frame = handle_tap_frame;

	if (tap_iface->head_size > ip_iface->head_size)
		ip_iface->head_size = tap_iface->head_size;
	if (ip_iface->head_size > tap_iface->head_size)
		tap_iface->head_size = ip_iface->head_size;

	tap_get_name ((struct iface *) tap_iface, ifname);
	printf ("Tunnel: %s\n", ifname);
	sprintf (cmd,
		 "/sbin/ip link set %s address %02x:%02x:%02x:%02x:%02x:%02x up\n",
		 ifname, ip_iface->hwaddr[0], ip_iface->hwaddr[1],
		 ip_iface->hwaddr[2], ip_iface->hwaddr[3], ip_iface->hwaddr[4], ip_iface->hwaddr[5]);
	printf ("command: %s\n", cmd);
	system (cmd);
	//sprintf( cmd, "/sbin/ip addr add fe80::1/64 dev %s", ifname );
	//printf( "command: %s\n", cmd );
	//system( cmd );
	//sprintf( cmd, "/sbin/ip route add %s/%d dev %s via fe80::5", prefix, plen, ifname );
	//printf( "command: %s\n", cmd );
	//system( cmd );
}

int
main (int argc, char **argv)
{
	int c;
	char *tapname = NULL;
	char *vnetname = "802ip";

	me = argv[0];

	setvbuf (stdout, NULL, _IONBF, 0);

	init_iface_drivers ();

	while ((c = getopt (argc, argv, "dI:i:")) != -1) {
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'i':
			tapname = optarg;
			break;
		case 'I':
			vnetname = optarg;
			break;
		default:
			usage ();
			break;
		}
	}

	init_ifs (tapname, vnetname);

	if (!debug)
		if (daemon (0, 0) < 0) {
			perror ("daemon");
			exit (1);
		}

	event_loop ();
	return 0;
}
