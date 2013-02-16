/*
 *  event.c
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>

#include "util.h"
#include "config.h"
#include "event.h"
#include "http_status.h"
#include "tcb.h"

static struct event *time_event_list = NULL;
static struct event *fd_event_list = NULL;
static struct event *always_event_list = NULL;

static int sigint_received = 0;

void
sigint_handler (int sig)
{
	++sigint_received;
	if (sigint_received > 1) {
		abort ();
	}
	signal (SIGINT, sigint_handler);
}

int
time_diff (time_ref * tr_start, time_ref * tr_end)
{
	return ((tr_end->tv_sec - tr_start->tv_sec) * 1000000 + tr_end->tv_usec - tr_start->tv_usec + 500) / 1000;
}

int
time_ago (time_ref * tr)
{
	struct timeval now;

	gettimeofday (&now, NULL);
	return time_diff (tr, &now);
}

void
time_now (time_ref * tr)
{
	gettimeofday ((struct timeval *) tr, NULL);
}

void
time_add (time_ref * tr, int msec)
{
	tr->tv_sec += msec / 1000;
	tr->tv_usec += (msec % 1000) * 1000;
}

void
time_future (time_ref * tr, int msec)
{
	gettimeofday (tr, NULL);
	time_add (tr, msec);
}

static struct event *
new_event (callback f, void *d)
{
	struct event *e;

	e = ALLOC (sizeof (struct event));
	e->next = NULL;
	e->prev = NULL;
	e->type = 0;
	e->remove = 0;
	e->func = f;
	e->data = d;
	return e;
}

static struct event *
del_event (struct event *e, struct event *list)
{
	if (e->next)
		e->next->prev = e->prev;
	if (e->prev)
		e->prev->next = e->next;
	if (e == list)
		return e->next;
	else
		return list;
}

struct event *
add_time_event (time_ref * t, callback f, void *d)
{
	struct event *e;

	e = new_event (f, d);
	e->type = EVENT_TIME;
	resched_time_event (e, t);
	return e;
}

void
resched_time_event (struct event *e, time_ref * tr)
{
	struct event *t;

	e->ev.time.time = *tr;
	e->remove = 0;
	/*syslog(LOG_DEBUG, "event at %ld sec, %ld usec", tr->tv_sec, tr->tv_usec );/**/
	time_event_list = del_event (e, time_event_list);
	if (!time_event_list) {
		time_event_list = e;
		return;
	}
	for (t = time_event_list; t; t = t->next) {
		if (time_diff (&e->ev.time.time, &t->ev.time.time) > 0) {
			e->next = t;
			e->prev = t->prev;
			t->prev = e;
			if (e->prev)
				e->prev->next = e;
			else
				time_event_list = e;
			return;
		}
		else if (!t->next) {
			e->next = NULL;
			e->prev = t;
			t->next = e;
			return;
		}
	}
	// should never get here
}

struct event *
add_fd_event (int fd, int write, callback f, void *d)
{
	struct event *e;

	e = new_event (f, d);
	e->type = EVENT_FD;
	e->ev.fd.fd = fd;
	e->ev.fd.write = write;
	e->next = fd_event_list;
	if (e->next)
		e->next->prev = e;
	fd_event_list = e;
	return e;
}

struct event *
add_always_event (callback f, void *d)
{
	struct event *e;

	e = new_event (f, d);
	e->type = EVENT_ALWAYS;
	e->next = always_event_list;
	if (e->next)
		e->next->prev = e;
	always_event_list = e;
	return e;
}

void
remove_event (struct event *e)
{
	switch (e->type) {
	case EVENT_TIME:
		time_event_list = del_event (e, time_event_list);
		break;
	case EVENT_FD:
		fd_event_list = del_event (e, fd_event_list);
		break;
	case EVENT_ALWAYS:
		always_event_list = del_event (e, always_event_list);
		break;
	}
	/*FREE(e);/* TODO: Find out why this crashes. */
}

void
event_loop (void)
{
	struct timeval t;
	struct event *e;
	int diff, highfd, ret, status_sock = -1;
	fd_set rfds, wfds;

	if (globals.http_port)
		status_sock = svr_sock (globals.http_port);

	for (;;) {
		FD_ZERO (&rfds);
		FD_ZERO (&wfds);
		highfd = -1;

		if (status_sock >= 0) {
			FD_SET (status_sock, &rfds);
			highfd = max_int (status_sock, highfd);
		}

		/* This is all so ugly...  It should use poll() eventually. */
		for (e = fd_event_list; e; e = e->next) {
			FD_SET (e->ev.fd.fd, e->ev.fd.write ? &wfds : &rfds);
			highfd = max_int (e->ev.fd.fd, highfd);
		}

		if ((e = time_event_list)) {
			diff = -time_ago (&e->ev.time.time);
            /*syslog(LOG_DEBUG, "event in %d usec", diff );/**/
			if (diff > 0) {
				if (always_event_list)
					diff = 0;
				t.tv_sec = diff / 1000;
				t.tv_usec = (diff % 1000) * 1000;
				ret = select (highfd + 1, &rfds, &wfds, NULL, &t);
			}
			else
				ret = 0;
			// if we have an immediate event, or select timed out,
			// we need to run the first time event *now*
			if (ret == 0) {
				e->remove = 1;
				(*e->func) (e, e->data);
				if (e->remove)
					remove_event (e);
			}
		}
		else
			ret = select (highfd + 1, &rfds, &wfds, NULL, NULL);

		if (ret > 0) {
			for (e = fd_event_list; e;) {
				if (FD_ISSET (e->ev.fd.fd, e->ev.fd.write ? &wfds : &rfds)
				    && !(*e->func) (e, e->data)) {
					struct event *n = e->next;
					remove_event (e);
					e = n;
				}
				else
					e = e->next;
			}
			if (status_sock >= 0 && FD_ISSET (status_sock, &rfds)) {
				write_status_report (status_sock);
			}
		}

		for (e = always_event_list; e;) {
			if (!(*e->func) (e, e->data)) {
				struct event *n = e->next;
				remove_event (e);
				e = n;
			}
			else
				e = e->next;
		}
		if (sigint_received) {
			syslog (LOG_NOTICE, "SIGINT received. Shutting down...");
			break;
		}
	}

	while (time_event_list)
		time_event_list = del_event (time_event_list, time_event_list);

	while (fd_event_list)
		fd_event_list = del_event (fd_event_list, fd_event_list);

	while (always_event_list)
		always_event_list = del_event (always_event_list, always_event_list);

}
