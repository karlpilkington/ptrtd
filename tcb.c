/*
 *  tcb.c
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
#include <sys/time.h>
#include <stdio.h>

#include "util.h"
#include "config.h"
#include "defs.h"
#include "buffer.h"
#include "tcb.h"

struct rb_root tcblist = RB_ROOT;

static int
tcbcmp (struct tcb const *a, struct tcb const *b)
{
	int rc = b->rport - a->rport;
	if (rc == 0) {
		rc = b->lport - a->lport;
		if (rc == 0) {
			rc = memcmp (a->raddr, b->raddr, 16);
			if (rc == 0) {
				rc = memcmp (a->raddr, b->raddr, 16);
			}
		}
	}
	return rc;
}

static int
tcb_insert (struct rb_root *root, struct tcb *tcb)
{
	struct rb_node **new = &(root->rb_node), *parent = 0;

	while (*new) {
		struct tcb *this = container_of (*new, struct tcb, node);
		int result = tcbcmp (tcb, this);

		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result > 0)
			new = &((*new)->rb_right);
		else
			return 0;
	}

	rb_link_node (&tcb->node, parent, new);
	rb_insert_color (&tcb->node, root);

	return 1;
}

static struct tcb *
tcb_find_rb (struct rb_root const *root, struct tcb const *tcb)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct tcb *data = container_of (node, struct tcb, node);
		int result = tcbcmp (tcb, data);

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return data;
	}

	return 0;
}

struct tcb *
tcb_new (uchar * laddr, int lport, uchar * raddr, int rport)
{
	struct tcb *t;

	t = ALLOC (sizeof (struct tcb));
	memset (t, 0, sizeof (struct tcb));

	if (laddr)
		memcpy (t->laddr, laddr, 16);

	if (raddr)
		memcpy (t->raddr, raddr, 16);

	t->lport = lport;
	t->rport = rport;

	gettimeofday (&t->start_time, NULL);

	if (globals.debug >= 5) {
		char fn[128];

		sprintf (fn, "session.%d", rport);
		if (!(t->fp = fopen (fn, "w"))) {
			perror ("fopen");
			exit (1);
		}
		setvbuf (t->fp, NULL, _IONBF, 0);
	}

	tcb_insert (&tcblist, t);

	return t;
}

void
tcb_delete (struct tcb *t)
{
	if (t->inbuf)
		rb_delete (t->inbuf);
	if (t->outbuf)
		rb_delete (t->outbuf);
	if (t->fp)
		fclose (t->fp);

	rb_erase (&t->node, &tcblist);
	FREE (t);
}

struct tcb *
tcb_find (uchar * laddr, int lport, uchar * raddr, int rport)
{
	struct tcb *t;
	struct tcb needle;

	if (!laddr) {
		memset (needle.laddr, 0, 16);
	}
	else {
		memcpy (needle.laddr, laddr, 16);
	}
	if (!raddr) {
		memset (needle.raddr, 0, 16);
	}
	else {
		memcpy (needle.raddr, raddr, 16);
	}

	needle.lport = lport;
	needle.rport = rport;

	t = tcb_find_rb (&tcblist, &needle);

	if (t)
		return t;

	if (!raddr)
		return tcb_find (laddr, lport, NULL, 0);

	if (!laddr)
		return tcb_find (NULL, lport, NULL, 0);

	if (lport > 0)
		return tcb_find (NULL, 0, NULL, 0);

	return NULL;
}
