/*
 *  ptrtd-tcp.c
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

#include "util.h"
#include "config.h"
#include "event.h"
#include "defs.h"
#include "tcp.h"

struct tcp_map
{
	struct tcp_map *next;
	struct tcp_map *prev;

	int fd;
	struct tcb *tcb;

	FILE *fp;

	struct event *e_fd_read;
	struct event *e_fd_write;
};

static struct tcp_callback tcp_map_cb;
static struct tcp_map *tlist = NULL;
static int num_tcp_maps = 0;

static void
kill_map (struct tcp_map *map)
{
	if (map->fd >= 0)
		close (map->fd);
	if (map->e_fd_write)
		remove_event (map->e_fd_write);
	if (map->e_fd_read)
		remove_event (map->e_fd_read);
	if (map->next)
		map->next->prev = map->prev;
	if (map->prev)
		map->prev->next = map->next;
	if (tlist == map)
		tlist = map->next;
	fprintf (stderr, "number of TCP maps: %d\n", --num_tcp_maps);
	FREE (map);
}

static int
handle_fd_can_read (struct event *e, void *d)
{
	struct tcp_map *map = d;
	uchar tb[65536];
	int len;

	printf ("handle_fd_can_read called for fd=%d\n", map->fd);
	len = tcp_get_output_space (map->tcb);
	if (len > sizeof (tb))
		len = sizeof (tb);
	if (map->fp)
		fprintf (map->fp, "we'll try to read %d into buffer\n", len);
	if (len == 0)
		return 0;
	len = read (map->fd, tb, len);
	if (map->fp)
		fprintf (map->fp, "read %d from fd %d into buffer\n", len, map->fd);

	if (len <= 0) {
		if (len == -1) {
			if (errno == EAGAIN)
				return 1;
			perror ("read");
		}
		map->e_fd_read = NULL;
		tcp_close (map->tcb, 0);
		kill_map (map);
		return 0;
	}
	else {
		tcp_send (map->tcb, tb, len, 0);
		if (tcp_get_output_space (map->tcb) == 0) {
			map->e_fd_read = NULL;
			return 0;
		}
		else
			return 1;
	}
}

int
handle_fd_can_write (struct event *e, void *d)
{
	struct tcp_map *map = d;
	uchar tb[65536];
	int len;

	len = tcp_read (map->tcb, tb, sizeof (tb));

	if (len < sizeof (tb))
		map->e_fd_write = NULL;

	len = write (map->fd, tb, len);
	if (map->fp)
		fprintf (map->fp, "wrote %d to fd %d from buffer\n", len, map->fd);

	if (len <= 0) {
		if (len == -1) {
			if (errno == EAGAIN)
				return 1;
			perror ("read");
		}
		map->e_fd_write = NULL;
		tcp_close (map->tcb, 0);
		kill_map (map);
		return 0;
	}

	return map->e_fd_write != NULL;
}

static int
handle_fd_did_connect (struct event *e, void *d)
{
	struct tcp_map *map = d;
	int ret, len;

	map->e_fd_write = NULL;

	len = sizeof (ret);
	getsockopt (map->fd, SOL_SOCKET, SO_ERROR, &ret, &len);

	if (ret != 0) {
		errno = ret;
		perror ("connect (delayed)");
		tcp_close (map->tcb, 0);
		kill_map (map);
	}
	else
		tcp_accept (map->tcb);

	return 0;
}

static void
incoming (struct tcb *t, void **d, FILE * fp)
{
	struct tcp_map *map;
	struct sockaddr_in addr;

	map = ALLOC (sizeof (struct tcp_map));
	fprintf (stderr, "number of TCP maps: %d\n", ++num_tcp_maps);
	map->next = tlist;
	map->prev = NULL;
	if (map->next)
		map->next->prev = map;
	tlist = map;
	map->fp = fp;
	map->tcb = t;
	map->e_fd_read = map->e_fd_write = NULL;
	*d = map;

	addr.sin_family = AF_INET;
	memcpy (&addr.sin_addr.s_addr, tcp_get_laddr (t) + 12, 4);
	addr.sin_port = htons (tcp_get_lport (t));

	map->fd = socket (PF_INET, SOCK_STREAM, 0);
	if (map->fd < 0) {
		perror ("socket");
		tcp_close (t, 0);
		kill_map (map);
		return;
	}
	fcntl (map->fd, F_SETFL, O_NONBLOCK);
	if (connect (map->fd, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
		if (errno == EINPROGRESS) {
			map->e_fd_write = add_fd_event (map->fd, 1, handle_fd_did_connect, map);
		}
		else {
			perror ("connect");
			tcp_close (t, 0);
			kill_map (map);
		}
	}
	else
		tcp_accept (t);
}

static void
output_space (void *s, int space)
{
	struct tcp_map *map = (struct tcp_map *) s;

	if (space > 0 && !map->e_fd_read)
		map->e_fd_read = add_fd_event (map->fd, 0, handle_fd_can_read, map);
}

static void
can_read (void *s, int length)
{
	struct tcp_map *map = (struct tcp_map *) s;

	if (!map->e_fd_write)
		map->e_fd_write = add_fd_event (map->fd, 1, handle_fd_can_write, map);
}

static void
closing (void *s, int hard)
{
	struct tcp_map *map = (struct tcp_map *) s;

	kill_map (map);
}

void
ptrtd_tcp_init (void)
{
	tcp_map_cb.incoming_session = incoming;
	tcp_map_cb.output_buffer_space = output_space;
	tcp_map_cb.data_available = can_read;
	tcp_map_cb.closing = closing;

	tcp_listen (&tcp_map_cb, NULL, 0);
}
