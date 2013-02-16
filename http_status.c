
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"
#include "http_status.h"
#include "tcb.h"
#include "config.h"

static int update_count = 0;

static void
setnoblock (int fd)
{
	int opts = fcntl (fd, F_GETFL);
	opts |= O_NONBLOCK;
	fcntl (fd, F_SETFL, opts);
}

int
svr_sock (unsigned short port)
{
	struct sockaddr_in6 saddr;
	int sock = socket (AF_INET6, SOCK_STREAM, 0);
	int const one = 1;

	if (sock < 0) {
		fprintf (stderr, "Failed to open socket on line %d in %s\n", __LINE__, __FILE__);
		exit (1);
	}

	setnoblock (sock);
	 /**/ memset (&saddr, 0, sizeof (saddr));
	saddr.sin6_port = htons (port);

	setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));

	bind (sock, (struct sockaddr *) &saddr, sizeof (saddr));
	listen (sock, 5);

	return sock;
}

void
write_status_report (int sock)
{
	char buffer[4000] = { "" };
	int connection_count = 0, rc, fd;
	struct tcb const *tcb = (struct tcb const *) rb_first (&tcblist);
	struct linger const linger = { 1, 5 };

	fd = accept (sock, 0, 0);
	while (fd >= 0) {
		int const one = 1;
		time_t const current_time = time (0);

		read (fd, buffer, sizeof (buffer));

		snprintf (buffer, sizeof (buffer), "<html>\n<body>\n"
			  "<h1>NAT64 Status</h1><hr/>\n"
			  "<h2>Active Connections</h2>\n"
			  "<table border = \"1\">\n"
			  "<tr>\n" "<td>Remote</td>\n"
			  "<td>Remote Port</td>\n" "<td>Local IPv4</td>\n"
			  "<td>Local Port</td>\n" "<td>Packets</td>\n"
			  "<td>inbuf</td>\n" "<td>outbuf</td>\n" "<td>State</td>\n" "<td>Start Time</td>\n" "</tr>\n");
		rc = write (fd, buffer, strlen (buffer));
		/* The last one in the list is the sentinal */
		while (tcb) {
			time_t const start_time = tcb->start_time.tv_sec;
			char raddr[INET6_ADDRSTRLEN] = { "Unknown" };
			char laddr4[INET6_ADDRSTRLEN] = { "Unknown" };

			inet_ntop (AF_INET6, tcb->raddr, raddr, sizeof (raddr));
			inet_ntop (AF_INET, tcb->laddr + 12, laddr4, sizeof (laddr4));

			snprintf (buffer, sizeof (buffer), "<tr>\n"
				  "<td>%s</td>\n"
				  "<td>%d</td>\n"
				  "<td>%s</td>\n"
				  "<td>%d</td>\n"
				  "<td>%d</td>\n"
				  "<td>%d</td>\n"
				  "<td>%d</td>\n"
				  "<td>%s</td>\n"
				  "<td>%s</td>\n"
				  "</tr>\n", raddr, tcb->rport, laddr4,
				  tcb->lport, tcb->packets, tcb->inbuf
				  && tcb->inbuf->p ? rb_left (tcb->inbuf) : -1,
				  tcb->outbuf && tcb->outbuf->p ? rb_left (tcb->outbuf) : -1, stname[tcb->state], ctime (&start_time));
			rc = write (fd, buffer, strlen (buffer));

			++connection_count;
			tcb = (struct tcb const *) rb_next ((struct rb_node const *) tcb);
		}


		snprintf (buffer, sizeof (buffer), "</table>\n" "<p>Total Connections: %d</p><hr/>\n"
			  "<p>IPv4 Network Prefix:  %x:%x:%x:%x::/%d</p>" "<p>Current Time: %s</p>\n"
#ifdef TRACK_MEMORY
			  "<p>Heap Memory in use: %dk</p>\n"
#endif
			  "<p>%s: %s</p>\n"  "<p>PID: %ld</p>" "<p>Status Updates: %d</p>\n" "</body>\n</html>\n", connection_count,
			  ntohs (globals.prefix[0]),
			  ntohs (globals.prefix[1]),
			  ntohs (globals.prefix[2]), ntohs (globals.prefix[3]), globals.plen, ctime (&current_time),
#ifdef TRACK_MEMORY
			  g_allocated / 1024,
#endif
			  PACKAGE, VERSION, (long)getpid(), ++update_count);
		rc = write (fd, buffer, strlen (buffer));

		shutdown (fd, SHUT_WR);
		close (fd);
		fd = accept (sock, 0, 0);
	}
}
