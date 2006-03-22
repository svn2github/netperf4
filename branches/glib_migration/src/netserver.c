char netserver_id[]="\
@(#)netserver.c (c) Copyright 2006, Hewlett-Packard Company, $Id$";
/*

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WITH_GLIB
# include <glib.h>
#else
# error sorry, this netserver requires glib
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifndef HAVE_GETOPT_LONG
#include "missing/getopt.h"
#else
#include <getopt.h>
#endif


#include "netperf.h"
#include "netlib.h"
#include "netmsg.h"

extern struct msgs NS_Msgs;

struct msgs *np_msg_handler_base = &NS_Msgs;

int debug = 0;

/* gcc does not like intializing where here */
FILE *where;
xmlChar *my_nid=NULL;

#define NETPERF_HASH_BUCKETS 1

server_hash_t  netperf_hash[NETPERF_HASH_BUCKETS];
test_hash_t    test_hash[TEST_HASH_BUCKETS];

char *program_name;

/* Option flags and variables */

char      *oname = "stdout";                            /* --output */
FILE      *ofile = NULL;
int        want_quiet;                                  /* --quiet, --silent */
int        want_brief;                                  /* --brief */
int        want_verbose;                                /* --verbose */
gboolean   spawn_on_accept = TRUE;
gboolean   want_daemonize = TRUE;
gboolean   exit_on_control_eof = TRUE;
gboolean   stdin_is_socket = FALSE;
int        control_socket = -1;

GIOChannel *control_channel;

int        forground = 0;
char      *listen_port = NETPERF_DEFAULT_SERVICE_NAME;  /* --port */
guint16    listen_port_num = 0;                         /* --port */
char      *local_host_name       = NULL;
int        local_address_family  = AF_UNSPEC;
gboolean   need_setup = FALSE;

int        orig_argc;
gchar    **orig_argv;

char	local_host_name_buf[BUFSIZ];
char    local_addr_fam_buf[BUFSIZ];



gboolean set_port_number(gchar *option_name, gchar *option_value, gpointer data, GError **error) {

  listen_port = g_strdup(option_value);
  listen_port_num = (guint16)atoi(listen_port);

  return(TRUE);

}
gboolean set_local_address(gchar *option_name, gchar *option_value, gpointer data, GError **error) {
 
  break_args_explicit(option_value,local_host_name_buf,local_addr_fam_buf);
  if (local_host_name_buf[0]) {
    local_host_name = &local_host_name_buf[0];
  }
  if (local_addr_fam_buf[0]) {
    local_address_family = parse_address_family(local_addr_fam_buf);
    /* if only the address family was set, we may need to set the
       local_host_name accordingly. since our defaults are IPv4 this
       should only be necessary if we have IPv6 support raj
       2005-02-07 */  
#if defined (AF_INET6)
    if (!local_host_name_buf[0]) {
      strncpy(local_host_name_buf,"::0",sizeof(local_host_name_buf));
      local_host_name = &local_host_name_buf[0];
    }
#endif
  }

  return(TRUE);
}

gboolean set_output_destination(gchar *option_name, gchar *option_value, gpointer data, GError **error) {
  oname = g_strdup(option_value);
  ofile = fopen(oname, "w");
  if (!ofile) {
    g_fprintf(stderr,
	    "%s: cannot open %s for writing\n",
	    program_name,
	    option_value);
    exit(1);
  }
  return(TRUE);
}

gboolean do_closesocket(gchar *option_name, gchar *option_value, gpointer data, GError **error) {
  SOCKET toclose;

  toclose = atoi(option_value);
  g_fprintf(where,"was asked to close socket %d\n",toclose);
#ifdef G_OS_WIN32
  closesocket(toclose);
#else
  close(toclose);
#endif
  return(TRUE);
}


gboolean set_debug(gchar *option_name, gchar *option_value, gpointer data, GError **error) {
  if (option_value) {
    /* set to value */
    debug = atoi(option_value);
  }
  else {
    /* simple increment */
    debug++;
  }
  return(TRUE);
}



gboolean set_verbose(gchar *option_name, gchar *option_value, gpointer data, GError **error) {
  if (option_value) {
    /* set to value */
    want_verbose = atoi(option_value);
  }
  else {
    /* simple increment */
    want_verbose++;
  }
  return(TRUE);
}
  

gboolean show_version(gchar *option_name, gchar *option_value, gpointer data, GError **error) {
  g_printf("netserver: %s.%s.%s\n",
	   NETPERF_VERSION,
	   NETPERF_UPDATE,
	   NETPERF_FIX);
  exit(0);
}

gboolean
spawn_new_netserver(int argc, char *argv[], SOCKET control, SOCKET toclose) {

  int new_argc;
  int i,j;
  gboolean ret;
  gboolean need_nodaemonize = TRUE;
  gboolean need_nospawn = TRUE;
  gchar **new_argv;
  gchar control_string[10]; /* that should be more than enough
			       characters I hope */
  GPid new_pid;
  GError *error=NULL;

  /* while we will _probably_ be stripping things from the argument
     vector, we want to make sure we have enough to add the control FD
     specification and the --nospawn. */

  new_argv = g_new(gchar *,argc+6);

  for (i = 0, j = 0, new_argc = 0; i < argc; i++) {
    /* this actually isn't correct for the future, but for now it will
       suffice to test passing the control FD to the new process */
    if (strcmp(argv[i],"--daemonize") == 0) {
      if (need_nodaemonize) {
	/* replace it with "--nodaemonize" in the new argument vector */
	new_argv[j++] = g_strdup("--nodaemonize");
	new_argc++;
	need_nodaemonize = FALSE;
      }
      continue;
    }
    if (strcmp(argv[i],"--nodaemonize") == 0) {
      if (need_nodaemonize) {
	need_nodaemonize = FALSE;
      }
      else {
	/* we already have one, so skip it */
	continue;
      }
    }
    if (strcmp(argv[i],"--nospawn") == 0) {
      if (need_nospawn) {
	need_nospawn = FALSE;
      }
      else {
	/* skip it */
	continue;
      }
    }
    if (strcmp(argv[i],"--spawn") == 0) {
      if (need_nospawn) {
	/* replace it with --nospawn */
	new_argv[j++] = g_strdup("--nospawn");
	new_argc++;
	need_nospawn = FALSE;
      }
      continue;
    }
    /* it is exceedingly unlikely that some of these options will be
       in the original argument vector, but just in case */
    if ((strcmp(argv[i],"--control") == 0) ||
	(strcmp(argv[i],"--port") == 0) ||
	(strcmp(argv[i],"-p") == 0) ||
	(strcmp(argv[i],"-L") == 0) ||
	(strcmp(argv[i],"--local") == 0)) {
      /* we want to skip the option AND its argument */
      i++;
      continue;
    }
    /* otherwise, we just copy old to new */
    new_argc++;
    new_argv[j++] = g_strdup(argv[i]);

  }
  if (need_nospawn) {
    new_argv[j++] = g_strdup("--nospawn"); /* don't recurse :) */
    new_argc++;
  }
  if (need_nodaemonize) {
    new_argv[j++] = g_strdup("--nodaemonize"); /* don't recurse */
    new_argc++;
  }
  new_argv[j++] = g_strdup("--control");
  g_snprintf(control_string,sizeof(control_string),"%d",control);
  new_argv[j++] = g_strdup(control_string);
  new_argc += 2;
  new_argv[j++] = g_strdup("--closesocket");
  g_snprintf(control_string,sizeof(control_string),"%d",toclose);
  new_argv[j++] = g_strdup(control_string);
  new_argc += 2;
  new_argv[j] = NULL;

  if (debug) {
    g_fprintf(where,"spawning with new_argc of %d and:\n",new_argc);
    for (i = 0; i < new_argc; i++) {
      g_fprintf(where,"\targv[%d] %s\n",i,new_argv[i]);
    }
  }

  ret = g_spawn_async_with_pipes(NULL,         /* working dir */
				 new_argv,     /* argument vector */
				 NULL,         /* env ptr */
				 G_SPAWN_SEARCH_PATH |
				 G_SPAWN_LEAVE_DESCRIPTORS_OPEN, /* flags */
				 NULL,         /* setup func */
				 NULL,         /* its arg */
				 &new_pid,      /* new process' id */
				 NULL,         /* dont watch stdin */
				 NULL,         /* ditto stdout */
				 NULL,         /* ditto stderr */
				 &error);
  if (error) {
    g_warning("%s g_io_spawn_async_with_pipes %s %d %s\n",
	      __func__,
	      g_quark_to_string(error->domain),
	      error->code,
	      error->message);
    g_clear_error(&error);
  }
  return(TRUE);
}

/* make a call to g_spawn_async_with_pipes to let us disconnect, at
   least after a fashion, from the controlling terminal. we will
   though remove the daemonze option from the argument vector used to
   start the new process */

gboolean
daemonize(int argc, char *argv[]) {

  int new_argc;
  int i,j;
  gboolean ret;
  gboolean need_nodaemonize = TRUE;
  char **new_argv;
  GPid new_pid;
  GError *error=NULL;

  /* make sure we have room for the --nodeamonize option we will be adding */
  new_argv = g_new(gchar *,argc+1);

  /* it is slightly kludgy, but simply taking a --nodeamonize to the
     end of the argument vector should suffice.  as things get more
     sophisticated, we may want to strip certain options from the
     vector. raj 2006-03-14 */

  for (i = 0, j = 0, new_argc = 0; i < argc; i++) {
    if (strcmp(argv[i], "--daemonize") == 0) {
      if (need_nodaemonize) {
	/* replace with --nodaemonize" */
	new_argv[j++] = g_strdup("--nodaemonize");
	new_argc++;
	need_nodaemonize = FALSE;
      }
      continue;
    }
    /* I seriously doubt we'd ever hit this but still */
    if (strcmp(argv[i],"--nodaemonize") == 0) {
      if (need_nodaemonize) {
	need_nodaemonize = FALSE;
      }
      else {
	/* skip it */
	continue;
      }
    }

    /* otherwise, we just copy old to new */
    new_argc++;
    new_argv[j++] = g_strdup(argv[i]);
  }
  if (need_nodaemonize) {
    new_argv[j++] = g_strdup("--nodaemonize");
    new_argc++;
  }
  new_argv[j] = NULL;

  if (debug) {
    g_fprintf(where,"daemonizing with new_argc of %d and:\n",new_argc);
    for (i = 0; i < new_argc; i++) {
      g_fprintf(where,"\targv[%d] %s\n",i,new_argv[i]);
    }
  }

  ret = g_spawn_async_with_pipes(NULL,         /* working dir */
				 new_argv,     /* argument vector */
				 NULL,         /* env ptr */
				 G_SPAWN_SEARCH_PATH |
				 G_SPAWN_LEAVE_DESCRIPTORS_OPEN, /* flags */
				 NULL,         /* setup func */
				 NULL,         /* its arg */
				 &new_pid,      /* new process' id */
				 NULL,         /* dont watch stdin */
				 NULL,         /* ditto stdout */
				 NULL,         /* ditto stderr */
				 &error);
  if (error) {
    g_warning("%s g_io_spawn_async_with_pipes %s %d %s\n",
	      __func__,
	      g_quark_to_string(error->domain),
	      error->code,
	      error->message);
    g_clear_error(&error);
  }
  exit(0);
}

static void
usage (int status)
{
  printf ("%s - network-oriented performance benchmarking\n", program_name);
  printf ("Usage: %s [OPTION]... [FILE]...\n", program_name);
  printf ("\
Options:\n\
  -d, --debug                increase the debugging level\n\
  -h, --help                 display this help and exit\n\
  -L, --local host,af        bind control endpoint to host with family af\n\
  -o, --output NAME          send output to NAME instead of standard output\n\
  -p, --port port            bind control to portnumber port\n\
  -q, --quiet, --silent      inhibit usual output\n\
  --brief                    shorten output\n\
  -v, --verbose              print more information\n\
  -V, --version              output version information and exit\n\
");
  exit (status);
}



int
add_netperf_to_hash(server_t *new_netperf) {

  int hash_value;

  hash_value = 0;
  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(netperf_hash[hash_value].hash_lock);

  new_netperf->next = netperf_hash[hash_value].server;
  new_netperf->lock = netperf_hash[hash_value].hash_lock;
  netperf_hash[hash_value].server = new_netperf;

  NETPERF_MUTEX_UNLOCK(netperf_hash[hash_value].hash_lock);

  return(NPE_SUCCESS);
}


void
delete_netperf(const xmlChar *id)
{

  /* we presume that the id is of the form [a-zA-Z][0-9]+ and so will
     call atoi on id and mod that with the SERVER_HASH_BUCKETS */

  int hash_value;
  server_t  *server_pointer;
  server_t **prev_server;

  hash_value = 0;

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(netperf_hash[hash_value].hash_lock);

  prev_server    = &(netperf_hash[hash_value].server);
  server_pointer = netperf_hash[hash_value].server;
  while (server_pointer != NULL) {
    if (!xmlStrcmp(server_pointer->id,id)) {
      /* we have a match */
      *prev_server = server_pointer->next;
      free(server_pointer);
      break;
    }
    prev_server    = &(server_pointer->next);
    server_pointer = server_pointer->next;
  }

  NETPERF_MUTEX_UNLOCK(netperf_hash[hash_value].hash_lock);
}


server_t *
find_netperf_in_hash(const xmlChar *id)
{

  /* we presume that the id is of the form [a-zA-Z][0-9]+ and so will
     call atoi on id and mod that with the SERVER_HASH_BUCKETS */

  int hash_value;
  server_t *server_pointer;

  hash_value = 0;

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(netperf_hash[hash_value].hash_lock);

  server_pointer = netperf_hash[hash_value].server;
  while (server_pointer != NULL) {
    if (!xmlStrcmp(server_pointer->id,id)) {
      /* we have a match */
      break;
    }
    server_pointer = server_pointer->next;
  }

  NETPERF_MUTEX_UNLOCK(netperf_hash[hash_value].hash_lock);

  return(server_pointer);
}



static int
instantiate_netperf( int sock )
{
  int rc=NPE_BAD_VERSION;
  xmlChar  * from_nid;
  xmlDocPtr  message;
  xmlNodePtr msg;
  xmlNodePtr cur;
  server_t * netperf;

  NETPERF_DEBUG_ENTRY(debug,where);

  while (rc != NPE_SUCCESS) {
    rc = recv_control_message(sock,&message);
    if (rc > 0) { /* received a message */
      if (debug) {
        g_fprintf(where, "Received a control message to doc %p\n", message);
        fflush(where);
      }
      msg = xmlDocGetRootElement(message);
      if (msg == NULL) {
        g_fprintf(stderr,"empty document\n");
        xmlFreeDoc(message);
        rc = NPE_EMPTY_MSG;
      } else {
        cur = msg->xmlChildrenNode;
        if (xmlStrcmp(cur->name,(const xmlChar *)"version")!=0) {
          if (debug) {
            g_fprintf(where,
                    "%s: Received an unexpected message\n", __func__);
            fflush(where);
          }
          rc = NPE_UNEXPECTED_MSG;
        } else {
          /* require the caller to ensure the netperf isn't already around */
          my_nid   = xmlStrdup(xmlGetProp(msg,(const xmlChar *)"tonid"));
          from_nid = xmlStrdup(xmlGetProp(msg,(const xmlChar *)"fromnid"));

          if ((netperf = (server_t *)malloc(sizeof(server_t))) == NULL) {
            g_fprintf(where,"%s: malloc failed\n", __func__);
            fflush(where);
            exit(1);
          }
          memset(netperf,0,sizeof(server_t));
          netperf->id        = from_nid;
          netperf->sock      = sock;
          netperf->state     = NSRV_CONNECTED;
          netperf->state_req = NSRV_WORK;
#ifdef WITH_GLIB
	  netperf->thread_id       = NULL;
#else
          netperf->thread_id       = -1;
#endif
          netperf->next      = NULL;

          /* check the version */
          rc = ns_version_check(cur,message,netperf);
        }
      }
    } 
    else {
      if (debug) {
        g_fprintf(where,"%s: close connection remote close\n", __func__);
        fflush(where);
      }
      CLOSE_SOCKET(sock);
      exit(-1);
    }
    if (rc == NPE_SUCCESS) {
      add_netperf_to_hash(netperf);
    } else {
      free(netperf);
    }
    xmlFreeDoc(message);
  } /* end of while */
  return(rc);
}


static void
report_stuck_test_status(server_t *netperf)
{
  int           i;
  uint32_t      reported;
  uint32_t      requested;
  uint32_t      current;
  test_hash_t  *h;
  test_t       *test;

  for (i = 0; i < TEST_HASH_BUCKETS; i ++) {
    h = &test_hash[i];
    /* mutex locking is not required because only one 
       netserver thread looks at these data structures sgb */
    test = h->test;
    while (test != NULL) {
      reported  = test->state;
      current   = test->new_state;
      requested = test->state_req;
      if (current != requested) {
        if (current != TEST_ERROR) {
          report_test_status(test);
        }
      }
      if (current != reported) {
        report_test_status(test);
      }
      test = test->next;
    }
  }
}

static void
check_test_state()
{
  int           i;
  uint32_t      orig;
  uint32_t      new;
  int           rc;
  test_t       *test;
  test_hash_t  *h;
  xmlNodePtr    msg = NULL;
  xmlNodePtr    new_node;
  xmlChar      *id;
  server_t     *netperf;
  char          code[8];

  netperf = netperf_hash[0].server;

  for (i = 0; i < TEST_HASH_BUCKETS; i ++) {
    h = &test_hash[i];
    /* mutex locking is not required because only one 
       netserver thread looks at these data structures sgb */
    test = h->test;
    while (test != NULL) {
      orig = test->state;
      new  = test->new_state;
      if (orig != new) {
        /* report change in test state */
        if (debug) {
          g_fprintf(where,"%s:tid = %s  state %d  new_state %d\n",
                  __func__, test->id, orig, new);
          fflush(where);
        }
        switch (new) {
        case TEST_INIT:
          /* what kind of error checking do we want to add ? */
          msg = xmlNewNode(NULL,(xmlChar *)"initialized");
          xmlSetProp(msg,(xmlChar *)"tid",test->id);
          new_node = xmlCopyNode(test->dependent_data,1);
          xmlAddChild(msg,new_node);
          break;
        case TEST_IDLE:
          msg = xmlNewNode(NULL,(xmlChar *)"idled");
          xmlSetProp(msg,(xmlChar *)"tid",test->id);
          break;
        case TEST_LOADED:
          msg = xmlNewNode(NULL,(xmlChar *)"loaded");
          xmlSetProp(msg,(xmlChar *)"tid",test->id);
          break;
        case TEST_MEASURE:
          msg = xmlNewNode(NULL,(xmlChar *)"measuring");
          xmlSetProp(msg,(xmlChar *)"tid",test->id);
          break;
        case TEST_ERROR:
          msg = xmlNewNode(NULL,(xmlChar *)"error");
          xmlSetProp(msg,(xmlChar *)"tid",test->id);
          xmlSetProp(msg,(xmlChar *)"err_fn",(xmlChar *)test->err_fn);
          xmlSetProp(msg,(xmlChar *)"err_str",(xmlChar *)test->err_str);
          sprintf(code,"%d",test->err_rc);
          xmlSetProp(msg,(xmlChar *)"err_rc",(xmlChar *)code);
          sprintf(code,"%d",test->err_no);
          xmlSetProp(msg,(xmlChar *)"err_no",(xmlChar *)code);
          break;
        case TEST_DEAD:
          msg = xmlNewNode(NULL,(xmlChar *)"dead");
          xmlSetProp(msg,(xmlChar *)"tid",test->id);
          id = test->id;
          break;
        default:
          break;
        }
        test->state = new;
        if (msg) {
          rc = send_control_message(netperf->sock, msg, netperf->id, my_nid);
          if (rc != NPE_SUCCESS) {
            if (debug) {
              g_fprintf(where,
                      "%s: send_control_message failed\n", __func__);
              fflush(where);
            }
          }
        }
      }
      test = test->next;
      if (new == TEST_DEAD) {
        delete_test(id);
      }
    }
    /* mutex unlocking is not required because only one 
       netserver thread looks at these data structures sgb */
  }
}



static void
kill_all_tests()
{
  int           i;
  int           empty_hash_buckets;
  test_t       *test;
  test_hash_t  *h;


  for (i = 0; i < TEST_HASH_BUCKETS; i ++) {
    h = &test_hash[i];
    /* mutex locking is not required because only one 
       netserver thread looks at these data structures sgb */
    test = h->test;
    while (test != NULL) {
      /* tell each test to die */
      test->state_req = TEST_DEAD;
      test = test->next;
    }
  }
  empty_hash_buckets = 0;
  while(empty_hash_buckets < TEST_HASH_BUCKETS) {
    empty_hash_buckets = 0;
    g_usleep(1000000);
    check_test_state();
    for (i = 0; i < TEST_HASH_BUCKETS; i ++) {
      if (test_hash[i].test == NULL) {
        empty_hash_buckets++;
      }
    }
  }
}


static int
close_netserver()
{
  int rc;
  int loop;
  server_t     *netperf;


  netperf = netperf_hash[0].server;
  if ((netperf->state_req == NSRV_CLOSE) ||
      (netperf->state_req == NSRV_EXIT ) ||
      (netperf->err_rc == NPE_REMOTE_CLOSE)) {
    xmlNodePtr    msg = NULL;
    kill_all_tests();
    msg = xmlNewNode(NULL,(xmlChar *)"closed");
    if (netperf->state_req == NSRV_CLOSE) {
      xmlSetProp(msg,(xmlChar *)"flag",(const xmlChar *)"LOOPING");
      loop = 1;
    }
    if (netperf->state_req == NSRV_EXIT) {
      xmlSetProp(msg,(xmlChar *)"flag",(const xmlChar *)"GONE");
      loop = 0;
    }
    if (msg) {
      rc = send_control_message(netperf->sock, msg, netperf->id, my_nid);
      if (rc != NPE_SUCCESS) {
        if (debug) {
          g_fprintf(where,
                  "%s: send_control_message failed\n", __func__);
          fflush(where);
        }
      }
    }
    delete_netperf(netperf->id);
  } else {
    /* we should never really get here   sgb  2005-12-06 */
    g_fprintf(where, "%s entered through some unknown path!!!!\n", __func__);
    g_fprintf(where, "netperf->state_req = %d \t netperf->err_rc = %d\n",
            netperf->state_req, netperf->err_rc);
    fflush(where);
    exit(-2);
  }
  return(loop);
}


static int
handle_netperf_requests(int sock)
{
  int rc = NPE_SUCCESS;
  struct pollfd fds;
  xmlDocPtr     message;
  server_t     *netperf;

  NETPERF_DEBUG_ENTRY(debug,where);

  rc = instantiate_netperf(sock);
  if (rc != NPE_SUCCESS) {
    g_fprintf(where,
            "%s %s: instantiate_netperf  error %d\n",
            program_name,
            __func__,
            rc);
    fflush(where);
    exit(rc);
  }
  netperf = netperf_hash[0].server;
  /* mutex locking is not required because only one 
     netserver thread looks at these data structures sgb */
  while(netperf->state != NSRV_ERROR) {

    /* check the state of all tests */
    check_test_state();

    fds.fd      = netperf->sock;
    fds.events  = POLLIN;
    fds.revents = 0;
    /* mutex unlocking is not required because only one 
       netserver thread looks at these data structures sgb */
    if (poll(&fds,1,5000) > 0) {
      /* mutex locking is not required because only one 
         netserver thread looks at these data structures sgb */
      rc = recv_control_message(netperf->sock, &message);
      /* mutex unlocking is not required because only one 
         netserver thread looks at these data structures sgb */
      if (rc > 0) {
        rc = process_message(netperf, message);
        if (rc) {
          g_fprintf(where,"process_message returned %d  %s\n",
                  rc, netperf_error_name(rc));
          fflush(where);
        }
      } else {
        netperf->state = NSRV_ERROR;
        netperf->err_fn = (char *)__func__;
        if (rc == 0) {
          netperf->err_rc = NPE_REMOTE_CLOSE;
        } else {
          g_fprintf(where,"recv_control_message returned %d  %s\n",
                  rc, netperf_error_name(rc));
          fflush(where);
          netperf->err_rc = rc;
        }
      }
    } else {
      if (debug) {
        g_fprintf(where,"ho hum, nothing to do\n");
        fflush(where);
        report_servers_test_status(netperf);
      }
      report_stuck_test_status(netperf);
    }
    /* mutex locking is not required because only one 
       netserver thread looks at these data structures sgb */
    if (rc != NPE_SUCCESS) {
      netperf->state  = NSRV_ERROR;
      netperf->err_rc = rc;
      netperf->err_fn = (char *)__func__;
    }
    if ((netperf->state_req == NSRV_CLOSE) ||
        (netperf->state_req == NSRV_EXIT ))  {
      break;
    }
  }

  if (rc != NPE_SUCCESS) {
    report_server_error(netperf);
  }
  /* mutex unlocking is not required because only one 
     netserver thread looks at these data structures sgb */
  return(close_netserver());
}


/* called when it is time to accept a new control connection off the
   listen endpoint */
gboolean  accept_connection(GIOChannel *source,
			    GIOCondition condition,
			    gpointer data) {
  SOCKET control_socket;
  SOCKET listen_socket;
  gboolean ret;
  GIOStatus  status;
  GIOChannel *control_channel;
  GError   *error=NULL;
  global_state_t *global_state;
  guint   watch_id;

  g_fprintf(where,"accepting a new connection\n");

  global_state = data;

#ifdef G_OS_WIN32
  listen_socket = g_io_channel_win32_get_fd(source);
#else
  listen_socket = g_io_channel_unix_get_fd(source);
#endif

  control_socket = accept(listen_socket,NULL,0);
  if (control_socket == SOCKET_ERROR) {
    g_fprintf(where,
	      "%s: accept failed errno %d %s\n",
	      __func__,
	      errno,
	      strerror(errno));
    fflush(where);
    exit(-1);
  }
  if (debug) {
    g_fprintf(where,
	      "%s: accepted connection on sock %d\n",
	      __func__,
	      control_socket);
    fflush(where);
  }
    
  if (spawn_on_accept) {
    ret = spawn_new_netserver(orig_argc,orig_argv,
			      control_socket,listen_socket);
    CLOSE_SOCKET(control_socket);  /* don't want to leak descriptors */
  }
  else {
    /* for now, we will simply start processing requests the
       "original" way. later we will establish a recv callback for a
       proper glib event loop */
    g_fprintf(where,"accepted a connection but told not to spawn\n");
#ifdef G_OS_WIN32
    control_channel = g_io_channel_win32_new_socket(control_socket);
#else
    control_channel = g_io_channel_unix_new(control_socket);
#endif
    status = g_io_channel_set_flags(control_channel,G_IO_FLAG_NONBLOCK,&error);
    if (error) {
      g_warning("g_io_channel_set_flags %s %d %s\n",
		g_quark_to_string(error->domain),
		error->code,
		error->message);
      g_clear_error(&error);
    }
    g_print("status after set flags %d control_channel %p\n",status,control_channel);
    
    status = g_io_channel_set_encoding(control_channel,NULL,&error);
    if (error) {
      g_warning("g_io_channel_set_encoding %s %d %s\n",
		g_quark_to_string(error->domain),
		error->code,
		error->message);
      g_clear_error(&error);
    }
    
    g_print("status after set_encoding %d\n");
    
    g_io_channel_set_buffered(control_channel,FALSE);
    
    watch_id = g_io_add_watch(control_channel, 
			      G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
			      read_from_control_connection,
			      data);
    g_print("added watch id %d\n",watch_id);
  }

  return(TRUE);
}


static SOCKET
setup_listen_endpoint(char service[]) {

  struct sockaddr   name;
  struct sockaddr  *peeraddr     = &name;
  int               namelen      = sizeof(name);
  netperf_socklen_t peerlen      = namelen;
  SOCKET            sock;
  SOCKET            listenfd     = 0;
  int               loop         = 1;
  char              filename[PATH_MAX];

  NETPERF_DEBUG_ENTRY(debug,where);

  /*
     if we were given a port number on the command line open a socket.
     Otherwise, if fd 0 is a socket then assume netserver was called by
     inetd and the socket and connection where created by inetd.
     Otherwise, use the default service name to open a socket.
   */

  if (service != NULL) {
    /* we are not a child of inetd start a listen socket */
    listenfd = establish_listen(local_host_name,
                                service,
                                local_address_family,
                                &peerlen);

    if (listenfd == -1) {
      g_fprintf(where,"%s: failed to open listen socket\n", __func__);
      fflush(where);
      exit(1);
    }
  return(listenfd);
  }
  return(-1);
}
#ifdef notdef
    if (peerlen > namelen) {
      peeraddr = (struct sockaddr *)malloc (peerlen);
    }

    if (!forground) {
      daemonize(orig_argc, orig_argv);
    }

    /* loopdeloop */

    while (loop) {
      printf("about to accept on socket %d\n",listenfd);
      if ((sock = accept(listenfd,peeraddr,&peerlen)) == -1) {
	g_fprintf(where,
		"%s: accept failed errno %d %s\n",
                __func__,
		errno,
		strerror(errno));
	fflush(where);
	exit(1);
      }
      if (debug) {
	g_fprintf(where,
		"%s: accepted connection on sock %d\n",
                __func__,
		sock);
	fflush(where);
      }

      /* OK, do we fork or not? */
      printf("at the fork in the road, forground was %d\n",forground);
      if (forground >= 2) {
	/* no fork please, eat with our fingers */
	printf("forground instantiation\n");
	/* if we get here, before we go back to call accept again,
	   probably a good idea to close the sock. of course, I'm not
	   even sure we will ever get here, but hey, what the heck.
	   raj 2005-11-10 */
	loop = handle_netperf_requests(sock);
	printf("closing sock %d\n",sock);
	close(sock);
      }
      else {
	/* we would like a fork please */
	printf("please hand me a fork\n");
	switch(fork()) {
	case -1:
	  /* not enough tines on our fork I suppose */
	  perror("netserver fork failure");
	  exit(-1);
	case 0:
	  /* we are the child, go ahead and handle netperf requests,
	     however, we really don't need the listenfd to be open, so
	     go ahead and close it */
	  printf("child closing %d\n",listenfd);
	  close(listenfd);

	  /* switch-over to our own pid-specific logfile */
	  snprintf(filename,
		   PATH_MAX,
		   "%s%s%d%s",
		   NETPERF_DEBUG_LOG_DIR,
		   NETPERF_DEBUG_LOG_PREFIX,
		   getpid(),
		   NETPERF_DEBUG_LOG_SUFFIX);
	  
	  where = freopen(filename,"w",where);
	  if (NULL == where) {
	    g_fprintf(stderr,
		    "ERROR netserver could not freopen where to %s errno %d\n",
		    filename,
		    errno);
	    fflush(stderr);
	    exit(-1);
	  }
	  handle_netperf_requests(sock);
	  /* as the child, if we've no more requests to process - eg
	     the connection has closed etc, then we might as well just
	     exit. raj 2005-11-11 */
	  exit(0);
	  break;
	default:
	  /* we are the parent, close the socket we just accept()ed
	     and let things loop around again. raj 2005-11-10 */
	  close(sock);
	}
      }
    }
  }
}
#endif

/* Netserver initialization */
static void
netserver_init()
{
  int   i;
  int   rc;

  NETPERF_DEBUG_ENTRY(debug,where);


  for (i = 0; i < NETPERF_HASH_BUCKETS; i++) {
    netperf_hash[i].server = NULL;
#ifdef WITH_GLIB
    netperf_hash[i].hash_lock = g_mutex_new();
    if (NULL == netperf_hash[i].hash_lock) {
      /* not sure we will even get here */
      g_fprintf(where, "%s: g_mutex_new error \n",__func__);
      fflush(where);
      exit(-2);
    }
    netperf_hash[i].condition = g_cond_new();
    if (NULL == netperf_hash[i].condition) {
      /* not sure we will even get here */
      g_fprintf(where, "%s: g_cond_new error \n",__func__);
      fflush(where);
      exit(-2);
    }
#else
    netperf_hash[i].hash_lock = 
      (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));

    if (NULL == netperf_hash[i].hash_lock) {
      g_fprintf(where, "%s: unable to malloc a mutex \n",__func__);
      fflush(where);
      exit(-2);
    }

    rc = pthread_mutex_init(netperf_hash[i].hash_lock, NULL);
    if (rc) {
      g_fprintf(where, "%s: server pthread_mutex_init error %d\n", __func__, rc);
      fflush(where);
      exit(rc);
    }

    netperf_hash[i].condition = 
      (pthread_cond_t *)malloc(sizeof(pthread_cond_t));

    if (NULL == netperf_hash[i].condition) {
      g_fprintf(where, "%s: unable to malloc a pthread_cond_t \n",__func__);
      fflush(where);
      exit(-2);
    }
      
    rc = pthread_cond_init(netperf_hash[i].condition, NULL);
    if (rc) {
      g_fprintf(where, "%s: server pthread_cond_init error %d\n", __func__, rc);
      fflush(where);
      exit(rc);
    }
#endif
  }
 
  for (i = 0; i < TEST_HASH_BUCKETS; i ++) {
    test_hash[i].test = NULL;
#ifdef WITH_GLIB
    test_hash[i].hash_lock = g_mutex_new();
    if (NULL == test_hash[i].hash_lock) {
      /* not sure we will even get here */
      g_fprintf(where, "%s: g_cond_new error \n",__func__);
      fflush(where);
      exit(-2);
    }
    test_hash[i].condition = g_cond_new();
    if (NULL == test_hash[i].condition) {
      /* not sure we will even get here */
      g_fprintf(where, "%s: g_cond_new error \n",__func__);
      fflush(where);
      exit(-2);
    }
#else
    test_hash[i].hash_lock = 
      (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));

    if (NULL == test_hash[i].hash_lock) {
      g_fprintf(where, "%s: unable to malloc a mutex \n",__func__);
      fflush(where);
      exit(-2);
    }

    rc = pthread_mutex_init(test_hash[i].hash_lock, NULL);
    if (rc) {
      g_fprintf(where, "%s: test pthread_mutex_init error %d\n", __func__, rc);
      fflush(where);
      exit(rc);
    }
    test_hash[i].condition = 
      (pthread_cond_t *)malloc(sizeof(pthread_cond_t));

    if (NULL == test_hash[i].condition) {
      g_fprintf(where, "%s: unable to malloc a pthread_cond_t \n",__func__);
      fflush(where);
      exit(-2);
    }

    rc = pthread_cond_init(test_hash[i].condition, NULL);
    if (rc) {
      g_fprintf(where, "%s: test pthread_cond_init error %d\n", __func__, rc);
      fflush(where);
      exit(rc);
    }
#endif
  }

  netlib_init();

}


static GOptionEntry netserver_entries[] =
  {
    {"daemonize", 0, 0, G_OPTION_ARG_NONE, &want_daemonize, "When launched from a shell, disconnect from that shell (default)", NULL},
    {"nodaemonize", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &want_daemonize, "When launched from a shell, do not disconnect from that shell", NULL},
    {"spawn", 0, 0, G_OPTION_ARG_NONE, &spawn_on_accept, "Spawn a new process for each control connection (default)", NULL},
    {"nospawn", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &spawn_on_accept, "Do not spawn a new process for each control connection", NULL},
    {"control", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &control_socket, "The FD that should be used for the control channel (internal)", NULL},
    {"closesocket", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, do_closesocket,"Close the specified socket", NULL},
    {"local", 'L', 0, G_OPTION_ARG_CALLBACK, set_local_address, "Specify the local addressing for the control endpoint", NULL},
    {"output", 'o', 0, G_OPTION_ARG_CALLBACK, set_output_destination, "Specify where misc. output should go", NULL},
    {"port", 'p', 0, G_OPTION_ARG_CALLBACK, set_port_number, "Specify the port number for the control connection", NULL},
    {"debug", 'd', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, set_debug, "Set the level of debugging",NULL},
    {"version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, show_version, "Display the netserver version and exit", NULL},
    {"verbose", 'v', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, set_verbose, "Set verbosity level for output", NULL},
    {"brief", 'b', 0, G_OPTION_ARG_NONE, &want_brief, "Request brief output", NULL},
    {"quiet", 'q', 0, G_OPTION_ARG_NONE, &want_quiet, "Display no test banners", NULL},
    { NULL }
  };


int
main(int argc, char **argv)
{

  int i;
  struct sockaddr name;
  netperf_socklen_t namelen = sizeof(name);
  SOCKET sock, listen_sock;

  guint  watch_id;
  GOptionContext *option_context;
  gboolean ret;
  GError *error = NULL;
  GIOStatus status;
  GMainLoop *loop;
  global_state_t *global_state_ptr;

#ifdef G_OS_WIN32
  WSADATA	wsa_data ;

#endif
  /* make sure that where points somewhere to begin with */
  where = stderr;

#ifdef G_OS_WIN32
  /* Initialize the winsock lib ( version 2.2 ) */
  if ( WSAStartup(MAKEWORD(2,2), &wsa_data) == SOCKET_ERROR ){
    g_fprintf(where,"WSAStartup() failed : %d\n", GetLastError()) ;
    return 1 ;
  }
#endif
  
  program_name = argv[0];

#ifdef WITH_GLIB
  g_thread_init(NULL);
#endif

  /* save-off the initial command-line stuff in case we need it for
     daemonizing or spawning after an accept */
  orig_argc = argc;
  orig_argv = g_new(gchar *,argc);
  for (i = 0; i < argc; i++){
    orig_argv[i] = g_strdup(argv[i]);
  }

  option_context = g_option_context_new(" - netperf4 netserver options");
  g_option_context_add_main_entries(option_context,netserver_entries, NULL);
  ret = g_option_context_parse(option_context, &argc, &argv, &error);
  if (error) {
    /* it sure would be nice to know how to trigger the help output */
    g_error("%s g_option_context_parse %s %d %s\n",
	      __func__,
	      g_quark_to_string(error->domain),
	      error->code,
	      error->message);
    g_clear_error(&error);
  }

  /* if we daemonize, we will not be returning, and we will be
     starting a whole new instance of this binary, because that is what
     g_spawn_asyync_with_pipes() does. */
  if (want_daemonize) {
    daemonize(orig_argc,orig_argv);
  }

  /* if neither stdin, nor control_socket are sockets, we need to
     setup a listen endpoint and go from there, otherwise, we assume
     we are a child of inetd or launched from a parent netserver raj
     2006-03-16 */

  if (getsockname(0, &name, &namelen) == 0) {
    /* we are a child of inetd or the like */
    need_setup = FALSE;
    sock = 0;
  }
  else if (getsockname(control_socket, &name, &namelen) == 0) {
    /* we were spawned by a parent netserver */
    need_setup = FALSE;
    sock = control_socket;
  }
  else {
    need_setup = TRUE;
  }

  if (ofile) {
    /* send our stuff to wherever the user told us to. likely we need
       to revisit this in the face of spawning processes... */
    where = ofile;
  }
  else {
    /* figure-out where to send it */
    if (need_setup) {
      /* for now, we can spit stuff to stderr since we are not a child
	 of inetd */
      where = stderr;
    }
    else {
      /* we are a child of inetd, best move where someplace other than
	 stderr. this needs to be made a bit more clean one of these days */
      char debugfile[PATH_MAX];
      sprintf(debugfile,
	      "%s%s_%d%s",
	      NETPERF_DEBUG_LOG_DIR,
	      NETPERF_DEBUG_LOG_PREFIX,
	      getpid(),
	      NETPERF_DEBUG_LOG_SUFFIX);
      where = fopen(debugfile,"w");
      if (NULL == where) {
	/* we need to put a warning _somewhere_ but where? */
	exit(-1);
      }
      else {
	/* and since we are a child of inetd, we don't want any stray
	   stderr stuff from other packages going out on the control
	   connection, so we should point stderr at where. if there
	   happens to be anything waiting to be flushed, we probably
	   best not flush it or fclose stderr lest that push garbage
	   onto the control connection. raj 2006-02-16 */
	/* but it seems this is a no-no for HP-UX so we'll just ignore
	   it for now. raj 2006-02-21 */
	/* stderr = where; */
      }
    }
  }

  /* do the work */
  xmlInitParser();
  xmlKeepBlanksDefault(0);

  xmlDoValidityCheckingDefaultValue = 1;
  xmlLoadExtDtdDefaultValue |= XML_COMPLETE_ATTRS;

  /* initialize basic data structures and stuff that needs to be done
     whether we are a child of inetd or not. raj 2005-10-11 */
  netserver_init();

  loop = g_main_loop_new(NULL, FALSE);

  global_state_ptr                = g_malloc(sizeof(global_state_t));
  global_state_ptr->server_hash   = netperf_hash;
  global_state_ptr->test_hash     = test_hash;
  global_state_ptr->message_state = g_malloc(sizeof(message_state_t));
  global_state_ptr->is_netserver  = TRUE;
  global_state_ptr->loop          = loop;
  global_state_ptr->message_state->have_header = FALSE;
  global_state_ptr->message_state->bytes_received = 0;
  global_state_ptr->message_state->bytes_remaining = 4;
  global_state_ptr->message_state->buffer = NULL;

  if (need_setup) {
    listen_sock = setup_listen_endpoint(listen_port);
#ifdef G_OS_WIN32
    control_channel = g_io_channel_win32_new_socket(listen_sock);
#else
    control_channel = g_io_channel_unix_new(listen_sock);
#endif
    status = g_io_channel_set_flags(control_channel,G_IO_FLAG_NONBLOCK,&error);
    if (error) {
      g_warning("g_io_channel_set_flags %s %d %s\n",
		g_quark_to_string(error->domain),
		error->code,
		error->message);
      g_clear_error(&error);
    }
    g_print("status after set flags %d control_channel %p\n",status,control_channel);
    
    status = g_io_channel_set_encoding(control_channel,NULL,&error);
    if (error) {
      g_warning("g_io_channel_set_encoding %s %d %s\n",
		g_quark_to_string(error->domain),
		error->code,
		error->message);
      g_clear_error(&error);
    }
    
    g_print("status after set_encoding %d\n");
    
    g_io_channel_set_buffered(control_channel,FALSE);
    
    /* loop is passed just to have something passed */
    watch_id = g_io_add_watch(control_channel, 
			      G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
			      accept_connection,
			      global_state_ptr);
    g_print("added watch id %d\n",watch_id);

    g_print("Starting loop to accept stuff...\n");
    g_main_loop_run(loop);
    g_print("Came out of the main loop\n");

  }
  else {
    /* we used to call handle_netperf_requests(sock); here, now we use
       the loop, luke... */
#ifdef G_OS_WIN32
    control_channel = g_io_channel_win32_new_socket(control_socket);
#else
    control_channel = g_io_channel_unix_new(control_socket);
#endif
    status = g_io_channel_set_flags(control_channel,G_IO_FLAG_NONBLOCK,&error);
    if (error) {
      g_warning("g_io_channel_set_flags %s %d %s\n",
		g_quark_to_string(error->domain),
		error->code,
		error->message);
      g_clear_error(&error);
    }
    g_print("status after set flags %d control_channel %p\n",status,control_channel);
    
    status = g_io_channel_set_encoding(control_channel,NULL,&error);
    if (error) {
      g_warning("g_io_channel_set_encoding %s %d %s\n",
		g_quark_to_string(error->domain),
		error->code,
		error->message);
      g_clear_error(&error);
    }
    
    g_print("status after set_encoding %d\n");
    
    g_io_channel_set_buffered(control_channel,FALSE);
    
    /* loop is passed just to have something passed */
    watch_id = g_io_add_watch(control_channel, 
			      G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
			      read_from_control_connection,
			      global_state_ptr);
    g_print("added watch id %d\n",watch_id);

    g_print("Starting loop to process stuff...\n");
    g_main_loop_run(loop);
    g_print("Came out of the main loop\n");
  }
}

