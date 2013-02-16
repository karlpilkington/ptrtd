/*
 *  event.h
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

#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>

#ifndef _EVENT_H
#define _EVENT_H

#define EVENT_TIME		1
#define EVENT_FD		2
#define EVENT_ALWAYS		3

struct event;

typedef struct timeval time_ref;
typedef int (*callback) (struct event * e, void *d);

struct time_event
{
	time_ref time;
};

struct fd_event
{
	int fd;
	int write;		// 0 = read, 1 = write
};

struct event
{
	struct event *prev;
	struct event *next;
	callback func;
	void *data;
	int type;
	int remove;
	union
	{
		struct time_event time;
		struct fd_event fd;
	} ev;
};

int time_diff (time_ref * tr_start, time_ref * tr_end);
int time_ago (time_ref * tr);

void time_now (time_ref * tr);
void time_add (time_ref * tr, int msec);
void time_future (time_ref * tr, int msec);

struct event *add_time_event (time_ref * t, callback f, void *d);
void resched_time_event (struct event *e, time_ref * t);
struct event *add_fd_event (int fd, int write, callback f, void *d);
struct event *add_always_event (callback f, void *d);
void remove_event (struct event *e);
void event_loop (void);

void sigint_handler (int);

#endif /* EVENT_H */
