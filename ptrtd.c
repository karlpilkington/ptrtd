/*
 *  ptrtd.c
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
#include <getopt.h>
#include <syslog.h>

#include "config.h"
#include "event.h"
#include "defs.h"
#include "util.h"
#include "if.h"
#include "icmp.h"
#include "buffer.h"
#include "tcp.h"

struct globals globals = { 0, {0, 0, 0, 0, 0, 0, 0, 0}, 64, "/etc/nat64d.conf", 0 };

void ptrtd_tcp_init (void);
void ptrtd_udp_init (void);

struct iface *iface;

void
usage (char const *me)
{
	printf ("Usage: %s [OPTION]...\n", me);
	printf ("\t-C, --config-file path                      specifies the config file path\n");
	printf ("\t-d, --debug <debug level>                   sets debugging level to 0, 1, 2, 3, 4 or 5\n");
	printf ("\t-m, --method <log method>                   sets to 'stderr'\n");
	printf ("\t-i, --interface [<driver>:]<interface>      names the interface\n");
	printf ("\t-h, --help                                  display this help and exit\n");
	printf ("\t-v, --version                               display this version info and exit\n");
	printf ("\n");
	printf ("\t\tprefix defaults to fec0:0:0:ffff::/64\n");
	printf ("\n");
	printf ("Report bugs to reubenhwk@yahoo.com\n");
}

void
handle_packet (struct iface *i, struct pbuf *p)
{
	int sum;

	sum = make_cksum (p->d, p->dlen);
	switch (p->d[6]) {
	case 58:
		handle_icmp (p->d, p->dlen);
		break;
	case 6:
		handle_tcp (p->d, p->dlen);
		break;
	case 17:
		handle_udp (p->d, p->dlen);
		break;
	case 0:
		break;
	default:
		icmp_send_error (p->d, p->dlen);
	}
}

void
init_iface (char *type, char *dev)
{
	char cmd[256], ifname[256];
	int do_config = 0;

	iface = create_iface (type, dev, handle_packet);
	if (!iface) {
		syslog(LOG_ERR, "Unable to create %s interface.\n", type);
		exit (1);
	}
	if (!strcmp (type, "tap")) {
		tap_get_name (iface, ifname);
		do_config = 1;
	}
	if (!strcmp (type, "tun")) {
		tun_get_name (iface, ifname);
		do_config = 1;
	}
	if (do_config) {
		int rc = 0;
		syslog (LOG_INFO, "Tunnel: %s\n", ifname);
		sprintf (cmd, "/sbin/ip link set %s up", ifname);
		syslog (LOG_INFO, "command: %s\n", cmd);
		rc = system (cmd);
		sprintf (cmd, "/sbin/ip addr add fe80::1/64 dev %s", ifname);
		syslog (LOG_INFO, "command: %s\n", cmd);
		rc = system (cmd);
		sprintf (cmd, "/sbin/ip route add %x:%x:%x:%x::/%d dev %s via fe80::5",
			 ntohs (globals.prefix[0]),
			 ntohs (globals.prefix[1]), ntohs (globals.prefix[2]), ntohs (globals.prefix[3]), globals.plen, ifname);
		syslog (LOG_INFO, "command: %s\n", cmd);
		rc = system (cmd);
	}
	icmp_init_iface (iface);
}

int
main (int argc, char **argv)
{
	char *itype = "tap", *iname = NULL;
	char temp[INET6_ADDRSTRLEN];
	struct option long_options[] = {
		{"config-file", required_argument, 0, 'C'},
		{"debug", required_argument, 0, 'd'},
		{"method", required_argument, 0, 'm'},
		{"interface-name", required_argument, 0, 'i'},
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
	};
	int c = 0, logopt = 0, option_index = 0;

	strtoip6 ((uchar *) globals.prefix, "fec0:0:0:ffff::");

	while ((c = getopt_long (argc, argv, "m:d:i:hvC:", long_options, &option_index)) != -1) {
		switch (c) {
		case 'C':
			globals.config_file = optarg;
			break;
		case 'd':
			globals.debug = atoi (optarg);
			break;
		case 'i':
			if ((iname = strchr (optarg, ':'))) {
				*iname = 0;
				++iname;
				itype = optarg;
			}
			else
				iname = optarg;
			break;
		case 'v':
			printf ("%s %s\n", PACKAGE, VERSION);
			exit (0);
			break;
        case 'm':
            if(strcmp(optarg, "stderr") == 0)
                logopt |= LOG_PERROR;
            break;
		case 'h':
		default:
			usage (argv[0]);
			exit (1);
			break;
		}
	}

    openlog (PACKAGE, logopt, LOG_DAEMON);

	read_config (globals.config_file);

	setvbuf (stdout, NULL, _IONBF, 0);

	init_iface_drivers ();

	ip6tostr (temp, sizeof (temp), (uchar const *) globals.prefix);
	syslog (LOG_INFO, "using prefix %s/%d\n", temp, globals.plen);

	ptrtd_tcp_init ();
	ptrtd_udp_init ();

	init_iface (itype, iname);

	if (!globals.debug) {
		if (daemon (0, 0) < 0) {
			perror ("daemon");
			exit (1);
		}
	}

	signal (SIGINT, sigint_handler);
	event_loop ();
	ptrtd_udp_finish ();

    closelog();

	return 0;
}
