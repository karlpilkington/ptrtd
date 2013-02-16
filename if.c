/*
 *  if.c
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

#include "util.h"
#include "config.h"
#include "defs.h"
#include "if.h"

struct ifdrv
{
	struct ifdrv *next;
	char name[128];
	struct iface *(*create_func) (char *arg, void (*handle_pkt) (struct iface * iface, struct pbuf * pkt));
};

static struct ifdrv *drvlist = NULL;

int
register_iface_driver (char *name, struct iface *(*create_func) (char *arg, void (*handle_pkt)




								   (struct iface * iface, struct pbuf * pkt)))
{
	struct ifdrv *id;

	id = ALLOC (sizeof (struct ifdrv));
	id->next = drvlist;
	drvlist = id;
	strncpy (id->name, name, 128);
	printf ("registering driver %s\n", name);
	id->create_func = create_func;
	return 0;
}

struct iface *
create_iface (char *name, char *arg, void (*handle_pkt) (struct iface * iface, struct pbuf * pkt))
{
	struct ifdrv *id;

	for (id = drvlist; id; id = id->next)
		if (!strcmp (id->name, name))
			return (*id->create_func) (arg, handle_pkt);

	return NULL;
}

void
init_iface_drivers (void)
{
	tuntap_register ();
	_802ip_register ();
	uml_sw_register ();
}
