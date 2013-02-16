/*
 *  if_tuntap.c
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
#include <linux/if.h>
#include <linux/if_ether.h>
#include <errno.h>

#include "config.h"

#ifdef HAVE_LINUX_IF_TUN_H
#include <linux/if_tun.h>
#endif

#include "util.h"
#include "defs.h"
#include "if.h"
#include "ether.h"

/**********************************************************************
 *                            COMMON CODE                             *
 **********************************************************************/

#ifdef HAVE_LINUX_IF_TUN_H
/* called from tun_new_if */
static int
get_tun (char *dev, char *reqdev, int tap)
{
	int fd;
	struct ifreq ifr;

	if ((fd = open ("/dev/net/tun", O_RDWR)) < 0) {
		perror ("tuntap open");
		exit (1);
	}
	memset (&ifr, 0, sizeof (ifr));
	ifr.ifr_flags = tap ? IFF_TAP : IFF_TUN;
	if (reqdev)
		strcpy (ifr.ifr_name, reqdev);
	if (ioctl (fd, TUNSETIFF, (void *) &ifr) < 0) {
		perror ("tun ioctl");
		exit (1);
	}
	strcpy (dev, ifr.ifr_name);
	return fd;
}
#else
/* called from tun_new_if */
static int
get_tun (char *dev, char *reqdev, int tap)
{
	int fd;
	char ifname[256];

	if (!reqdev || !reqdev[0]) {
		fprintf (stderr, "Please specify a device name with the -i flag.\n");
		exit (1);
	}
	sprintf (ifname, "/dev/%s", reqdev);
	if ((fd = open (ifname, O_RDWR)) < 0) {
		if (errno == ENOENT)
			fprintf (stderr, "Device %s does not exist!\n", reqdev);
		else
			perror ("tun open");
		exit (1);
	}
	strcpy (dev, reqdev);
	return fd;
}
#endif

/* called by tun/tap_read_callback */
static int
tuntap_do_read (int fd, struct pbuf *p)
{
	p->dlen = read (fd, p->d, p->max);
	if (p->dlen < 0) {
		perror ("read from tun");
		exit (1);
	}
	// does 0 mean the interface died?
#ifdef HAVE_LINUX_IF_TUN_H
	if (p->dlen > 4 && GET_16 (p->d + 2) == ETH_P_IPV6) {
		pbuf_drop (p, 4);
		return 0;
	}
#else
	if (p->dlen > 0) {
		return 0;
	}
#endif
	return -1;
}

/* called by tun/tap_send routines */
static int
tuntap_do_send (int fd, struct pbuf *p)
{
#ifdef HAVE_LINUX_IF_TUN_H
	pbuf_raise (p, 4);
	p->d[0] = p->d[1] = 0;
	PUT_16 (p->d + 2, ETH_P_IPV6);
#endif
	return write (fd, p->d, p->dlen) < 0 ? -1 : 0;
}

/**********************************************************************
 *                             TUN CODE                               *
 **********************************************************************/

struct iface_tun
{
	struct iface iface;
	void (*pkt_handler) (struct iface * iface, struct pbuf * pkt);
	int fd;
	char devname[256];
};

/* called from read event created by tun_new_if */
static int
tun_read_callback (struct event *e, void *d)
{
	struct iface_tun *iface = (struct iface_tun *) d;
	struct pbuf *pkt;

#ifdef HAVE_LINUX_IF_TUN_H
	pkt = pbuf_new (iface->iface.mtu + 4);
#else
	pkt = pbuf_new (iface->iface.mtu);
#endif
	if (tuntap_do_read (iface->fd, pkt) == 0) {
		(*iface->pkt_handler) ((struct iface *) iface, pkt);
	}
	pbuf_delete (pkt);
	return 1;
}

/* called by main code via pointer in struct iface */
static struct pbuf *
tun_get_buf (struct iface *iface, int size)
{
	struct pbuf *p;

	p = pbuf_new (size + 4);
	pbuf_drop (p, 4);
	return p;
}

/* called by main code via pointer in struct iface */
static int
tun_send (struct iface *iface, struct pbuf *p, uchar * d)
{
	int ret;

	ret = tuntap_do_send (((struct iface_tun *) iface)->fd, p);
	pbuf_delete (p);
	return ret;
}

/* called directly from iface driver registry */
struct iface *
tun_new_if (char *dev, void (*handle_pkt) (struct iface * iface, struct pbuf * pkt))
{
	struct iface_tun *iface;

#ifndef HAVE_LINUX_IF_TUN_H
	fprintf (stderr, "This version of the tuntap driver doesn't support IPv6 in tun mode.\nPlease use the tap interface.\n");
	exit (1);
#endif

	iface = ALLOC (sizeof (struct iface_tun));
	iface->fd = get_tun (iface->devname, dev, 0);
	fcntl (iface->fd, F_SETFL, O_NONBLOCK);
	iface->pkt_handler = handle_pkt;
	iface->iface.mtu = 1280;
	iface->iface.hwaddr_len = 0;
	iface->iface.get_buffer = tun_get_buf;
	iface->iface.send_unicast = tun_send;
	iface->iface.send_multicast = tun_send;
	add_fd_event (iface->fd, 0, tun_read_callback, iface);

	return (struct iface *) iface;
}

/* called directly from main code */
void
tun_get_name (struct iface *iface, char *name)
{
	strcpy (name, ((struct iface_tun *) iface)->devname);
}

/**********************************************************************
 *                             TAP CODE                               *
 **********************************************************************/

struct iface_tap
{
	struct iface_ether ife;
	int fd;
	char devname[256];
};

static const char default_mac[6] = { 0x2, 0x0, 0x12, 0x34, 0x56, 0x78 };

/* called from read event created by tap_new_if */
static int
tap_read_callback (struct event *e, void *d)
{
	struct iface_tap *iface = (struct iface_tap *) d;
	struct pbuf *p;

	p = pbuf_new (iface->ife.iface.mtu + iface->ife.head_size);
	pbuf_drop (p, iface->ife.head_size - 14);
	tuntap_do_read (iface->fd, p);
	iface->ife.recv_frame (&iface->ife, p);
	pbuf_delete (p);
	return 1;
}

/* called by ethernet code via pointer in iface_ether */
static int
tap_send_frame (struct iface_ether *iface, struct pbuf *p)
{
	return tuntap_do_send (((struct iface_tap *) iface)->fd, p);
}

/* called directly from main code */
void
tap_get_name (struct iface *iface, char *name)
{
	strcpy (name, ((struct iface_tap *) iface)->devname);
}

/* called directly from iface driver registry */
struct iface *
tap_new_if (char *dev, void (*handle_pkt) (struct iface * iface, struct pbuf * pkt))
{
	struct iface_tap *iface;

	iface = ALLOC (sizeof (struct iface_tap));
	memset (iface, 0, sizeof (struct iface_tap));

	iface->fd = get_tun (iface->devname, dev, 1);
	fcntl (iface->fd, F_SETFL, O_NONBLOCK);

	ether_setup ((struct iface_ether *) iface);
	iface->ife.pkt_handler = handle_pkt;
	iface->ife.send_frame = tap_send_frame;
	iface->ife.head_size += 4;
	memcpy (iface->ife.hwaddr, default_mac, 6);
	iface->ife.iface.mtu = 1280;

	add_fd_event (iface->fd, 0, tap_read_callback, iface);

	return (struct iface *) iface;
}

int
tuntap_register (void)
{
	register_iface_driver ("tun", tun_new_if);
	register_iface_driver ("tap", tap_new_if);
	return 0;
}
