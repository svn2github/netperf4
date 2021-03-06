/*

Copyright (c) 2005,2006 Hewlett-Packard Company 

$Id$

This file is part of netperf4.

Netperf4 is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

Netperf4 is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

In addition, as a special exception, the copyright holders give
permission to link the code of netperf4 with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the same
license as the "OpenSSL" library), and distribute the linked
executables.  You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".  If you modify
this file, you may extend this exception to your version of the file,
but you are not obligated to do so.  If you do not wish to do so,
delete this exception statement from your version.

*/

#ifndef _NETLIB_H
#define _NETLIB_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifndef G_OS_WIN32
#define SOCKET int
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>


extern  void delete_test(const xmlChar *id);
extern  test_t * find_test_in_hash(const xmlChar *id);
extern  void report_test_status(test_t *test);
extern  void report_servers_test_status(server_t *server);
extern  GenReport get_report_function(xmlNodePtr cmd);
extern  const char * netperf_error_name(int rc);
extern  char * npe_to_str(int npe_error);
extern  int set_test_locality(test_t  *test,
                              char *loc_type,
                              char *loc_value);
extern int set_thread_locality(void *thread_id,
			       char *loc_type,
			       char *loc_value,
			       int debug,
			       FILE *where);
extern  int set_own_locality(char *loc_type,
			     char *loc_value,
			     int debug,
			     FILE *where);
extern int clear_own_locality(char *loc_type,
			      int debug,
			      FILE *where);

#ifdef HAVE_GETHRTIME
extern void netperf_timestamp(hrtime_t *timestamp);
extern int  delta_micro(hrtime_t *begin, hrtime_t *end);
extern int  delta_milli(hrtime_t *begin, hrtime_t *end);
#else
extern void netperf_timestamp(struct timeval *timestamp);
extern int  delta_micro(struct timeval *begin,struct timeval *end);
extern int  delta_milli(struct timeval *begin,struct timeval *end);
#endif

extern int strtofam(xmlChar *familystr);
extern void dump_addrinfo(FILE *dumploc, struct addrinfo *info,
			  xmlChar *host, xmlChar *port, int family);
extern SOCKET establish_control(xmlChar *hostname,  xmlChar *port, int remfam,
			     xmlChar *localhost, xmlChar *localport, int locfam);
extern int get_test_function(test_t *test, const xmlChar *func);
extern int add_test_to_hash(test_t *new_test);
extern int write_to_control_connection(GIOChannel *channel, 
				       xmlNodePtr body,
				       xmlChar *nid,
				       const xmlChar *fromnid);
extern int32_t recv_control_message(SOCKET control_sock, xmlDocPtr *message);
extern void report_server_error(server_t *server);
extern int launch_thread(GThread **tid, void *(*start_routine)(void *), void *data);
extern void break_args_explicit(char *s, char *arg1, char *arg2);
extern int parse_address_family(char family_string[]);
extern SOCKET establish_listen(char *hostname, char *service, 
			    int af, netperf_socklen_t *addrlenp);

extern int netperf_complete_filename(char *name, char *full, int fulllen);
extern gboolean read_from_control_connection(GIOChannel *source, GIOCondition condition, gpointer data);
extern void *launch_pad(void *data);

/* state machine data structure for process message */

typedef int (*msg_func_t)(xmlNodePtr msg, xmlDocPtr doc, server_t *server);

struct msgs {
  char *msg_name;
  msg_func_t msg_func;
  unsigned int valid_states;
};

typedef struct message_state {
  gboolean have_header;
  gint32  bytes_received;
  gint32  bytes_remaining;
  gchar   *buffer;
} message_state_t;

typedef struct global_state {
  server_hash_t   *server_hash;   /* where to find netperf/netserver hash */
  test_hash_t     *test_hash;     /* where to find the test hash */
  message_state_t *message_state; /* so we can keep track of partials */
  GMainLoop       *loop;          /* so we can add things to the loop */
  gboolean        is_netserver;   /* not sure if this is really necessary */
  gboolean        first_message;  /* do we await the first message? */
} global_state_t;

typedef struct thread_launch_state {
  void *data_arg;                 /* the actual data to be passed */
  void *(*start_routine)(void *); /* the actual routine to execute */
} thread_launch_state_t;

extern void netlib_init();

void display_test_hash();
#endif
