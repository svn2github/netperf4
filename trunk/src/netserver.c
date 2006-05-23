char netserver_id[]="\
@(#)netserver.c (c) Copyright 2005, Hewlett-Packard Company, $Id$";
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

#include <termios.h>
#include <grp.h>
#include <pwd.h>

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WITH_GLIB
# ifdef HAVE_GLIB_H
#  include <glib.h>
# endif 
#else
# ifdef HAVE_PTHREAD_H
#  include <pthread.h>
# endif
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
int        forground = 0;
char      *listen_port = NETPERF_DEFAULT_SERVICE_NAME;  /* --port */
uint16_t   listen_port_num = 0;                         /* --port */
char      *local_host_name       = NULL;
int        local_address_family  = AF_UNSPEC;
int        need_setup = 0;

char	local_host_name_buf[BUFSIZ];
char    local_addr_fam_buf[BUFSIZ];

/* getopt_long return codes */
enum {DUMMY_CODE=129
      ,BRIEF_CODE
};

static struct option const long_options[] =
{
  {"output", required_argument, 0, 'o'},
  {"input", required_argument, 0, 'i'},
  {"config", required_argument, 0, 'c'},
  {"debug", no_argument, 0, 'd'},
  {"forground", no_argument, 0, 'f'},
  {"quiet", no_argument, 0, 'q'},
  {"local", required_argument, 0, 'L'},
  {"silent", no_argument, 0, 'q'},
  {"brief", no_argument, 0, BRIEF_CODE},
  {"verbose", no_argument, 0, 'v'},
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},
  {NULL, 0, NULL, 0}
};

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

/* Set all the option flags according to the switches specified.
   Return the index of the first non-option argument.  */

static int
decode_command_line (int argc, char **argv)
{
  int c;

  while ((c = getopt_long (argc, argv,
			   "d"  /* debug */
			   "f"  /* forground */
			   "L:" /* local control sock address info */
                           "o:" /* output */
                           "p:" /* port */
                           "q"  /* quiet or silent */
                           "v"  /* verbose */
                           "h"  /* help */
                           "V", /* version */
                           long_options, (int *) 0)) != EOF)
    {
      switch (c)
        {
	case 'd':       /* --debug */
	  debug++;
	  break;
        case 'f':       /* --forground */
          forground++;  /* 1 means no deamon, 2 means no fork after accept */
	  need_setup = 1;
          break;
	case 'L':
	  break_args_explicit(optarg,local_host_name_buf,local_addr_fam_buf);
	  if (local_host_name_buf[0]) {
            local_host_name = &local_host_name_buf[0];
	  }
	  if (local_addr_fam_buf[0]) {
	    local_address_family = parse_address_family(local_addr_fam_buf);
	    /* if only the address family was set, we may need to set the
	       local_host_name accordingly. since our defaults are IPv4
	       this should only be necessary if we have IPv6 support raj
	       2005-02-07 */  
#if defined (AF_INET6)
	    if (!local_host_name_buf[0]) {
	      strncpy(local_host_name_buf,"::0",sizeof(local_host_name_buf));
              local_host_name = &local_host_name_buf[0];
	    }
#endif
	  }
	  need_setup = 1;
	  break;
        case 'o':               /* --output */
          oname = strdup(optarg);
          ofile = fopen(oname, "w");
          if (!ofile) {
            fprintf(stderr,
                    "%s: cannot open %s for writing\n",
                    program_name,
                    optarg);
              exit(1);
            }
          break;
        case 'p':               
          /* we want to open a listen socket at a */
          /* specified port number */
          listen_port = strdup(optarg);
          listen_port_num = atoi(listen_port);
	  need_setup = 1;
          break;
        case 'q':               /* --quiet, --silent */
          want_quiet = 1;
          break;
        case BRIEF_CODE:        /* --brief */
          want_brief = 1;
          break;
        case 'v':               /* --verbose */
          want_verbose = 1;
          break;
        case 'V':
          printf ("netserver %s.%s.%s\n",
                  NETPERF_VERSION,
                  NETPERF_UPDATE,
                  NETPERF_FIX);
          exit (0);
        case 'h':
          usage (0);
        default:
          usage (EXIT_FAILURE);
        }
    }

  return optind;
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
        fprintf(where, "Received a control message to doc %p\n", message);
        fflush(where);
      }
      msg = xmlDocGetRootElement(message);
      if (msg == NULL) {
        fprintf(stderr,"empty document\n");
        xmlFreeDoc(message);
        rc = NPE_EMPTY_MSG;
      } else {
        cur = msg->xmlChildrenNode;
        if (xmlStrcmp(cur->name,(const xmlChar *)"version")!=0) {
          if (debug) {
            fprintf(where,
                    "%s: Received an unexpected message\n", __func__);
            fflush(where);
          }
          rc = NPE_UNEXPECTED_MSG;
        } else {
          /* require the caller to ensure the netperf isn't already around */
          my_nid   = xmlStrdup(xmlGetProp(msg,(const xmlChar *)"tonid"));
          from_nid = xmlStrdup(xmlGetProp(msg,(const xmlChar *)"fromnid"));

          if ((netperf = (server_t *)malloc(sizeof(server_t))) == NULL) {
            fprintf(where,"%s: malloc failed\n", __func__);
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
        fprintf(where,"%s: close connection remote close\n", __func__);
        fflush(where);
      }
      close(sock);
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

/* daemonize will fork a daemon into the background, freeing-up the
   shell prompt for other uses. I suspect that for non-Unix OSes,
   something other than fork() may be required? raj 2/98 */
void
daemonize()
{
  char filename[PATH_MAX];

  /* fork off - only the client will return, the parent will exit */
  
  switch (fork()) {
  case -1:
    /* something went wrong */
    perror("netserver fork error");
    exit(-1);
  case 0:
    /* we are the child process. set ourselves up as the session
       leader so we detatch from the parent, and then return to the
       caller so he can do whatever it is he wants to do */

    fclose(stdin);

    snprintf(filename,
	     PATH_MAX,
	     "%s%s%d%s",
	     NETPERF_DEBUG_LOG_DIR,
	     NETPERF_DEBUG_LOG_PREFIX,
	     getpid(),
	     NETPERF_DEBUG_LOG_SUFFIX);
    where = freopen(filename,"w",where);
    if (NULL == where) {
      fprintf(stderr,
	      "ERROR netserver could not freopen where to %s errno %d\n",
	      filename,
	      errno);
      fflush(stderr);
      exit(-1);
    }

    /* at this point should I close stderr?!? */
    setsid();

    /* not all OSes have SIGCLD as SIGCHLD */
#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif /* SIGCLD */
    
    signal(SIGCLD, SIG_IGN);

    break;
  default:
    /* we are the parent process - the one invoked from the
       shell. since the first child will detach itself, we can just go
       ahead and slowly fade away */
    exit(0);
  }
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
          fprintf(where,"%s:tid = %s  state %d  new_state %d\n",
                  __func__, test->id, orig, new);
          fflush(where);
        }
        switch (new) {
        case TEST_INIT:
        case TEST_IDLE:
          /* remore race condition of to quickly moving through TEST_INIT */
          if (orig == TEST_PREINIT) {
            /* what kind of error checking do we want to add ? */
            msg = xmlNewNode(NULL,(xmlChar *)"initialized");
            xmlSetProp(msg,(xmlChar *)"tid",test->id);
            new_node = xmlCopyNode(test->dependent_data,1);
            xmlAddChild(msg,new_node);
            new = TEST_INIT;
          }
          else {
            msg = xmlNewNode(NULL,(xmlChar *)"idled");
            xmlSetProp(msg,(xmlChar *)"tid",test->id);
          }
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
              fprintf(where,
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
    sleep(1);
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
          fprintf(where,
                  "%s: send_control_message failed\n", __func__);
          fflush(where);
        }
      }
    }
    delete_netperf(netperf->id);
  } else {
    /* we should never really get here   sgb  2005-12-06 */
    fprintf(where, "%s entered through some unknown path!!!!\n", __func__);
    fprintf(where, "netperf->state_req = %d \t netperf->err_rc = %d\n",
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
    fprintf(where,
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
          fprintf(where,"process_message returned %d  %s\n",
                  rc, netperf_error_name(rc));
          fflush(where);
        }
      } else {
        netperf->state = NSRV_ERROR;
        netperf->err_fn = (char *)__func__;
        if (rc == 0) {
          netperf->err_rc = NPE_REMOTE_CLOSE;
        } else {
          fprintf(where,"recv_control_message returned %d  %s\n",
                  rc, netperf_error_name(rc));
          fflush(where);
          netperf->err_rc = rc;
        }
      }
    } else {
      if (debug) {
        fprintf(where,"ho hum, nothing to do\n");
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



static void
setup_listen_endpoint(char service[]) {

  struct sockaddr   name;
  struct sockaddr  *peeraddr     = &name;
  int               namelen      = sizeof(name);
  netperf_socklen_t peerlen      = namelen;
  int               sock;
  int               listenfd     = 0;
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
      fprintf(where,"%s: failed to open listen socket\n", __func__);
      fflush(where);
      exit(1);
    }

    if (peerlen > namelen) {
      peeraddr = (struct sockaddr *)malloc (peerlen);
    }

    if (!forground) {
      daemonize();
    }

    /* loopdeloop */

    while (loop) {
      printf("about to accept on socket %d\n",listenfd);
      if ((sock = accept(listenfd,peeraddr,&peerlen)) == -1) {
	fprintf(where,
		"%s: accept failed errno %d %s\n",
                __func__,
		errno,
		strerror(errno));
	fflush(where);
	exit(1);
      }
      if (debug) {
	fprintf(where,
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
	    fprintf(stderr,
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
      fprintf(where, "%s: g_mutex_new error \n",__func__);
      fflush(where);
      exit(-2);
    }
    netperf_hash[i].condition = g_cond_new();
    if (NULL == netperf_hash[i].condition) {
      /* not sure we will even get here */
      fprintf(where, "%s: g_cond_new error \n",__func__);
      fflush(where);
      exit(-2);
    }
#else
    netperf_hash[i].hash_lock = 
      (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));

    if (NULL == netperf_hash[i].hash_lock) {
      fprintf(where, "%s: unable to malloc a mutex \n",__func__);
      fflush(where);
      exit(-2);
    }

    rc = pthread_mutex_init(netperf_hash[i].hash_lock, NULL);
    if (rc) {
      fprintf(where, "%s: server pthread_mutex_init error %d\n", __func__, rc);
      fflush(where);
      exit(rc);
    }

    netperf_hash[i].condition = 
      (pthread_cond_t *)malloc(sizeof(pthread_cond_t));

    if (NULL == netperf_hash[i].condition) {
      fprintf(where, "%s: unable to malloc a pthread_cond_t \n",__func__);
      fflush(where);
      exit(-2);
    }
      
    rc = pthread_cond_init(netperf_hash[i].condition, NULL);
    if (rc) {
      fprintf(where, "%s: server pthread_cond_init error %d\n", __func__, rc);
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
      fprintf(where, "%s: g_cond_new error \n",__func__);
      fflush(where);
      exit(-2);
    }
    test_hash[i].condition = g_cond_new();
    if (NULL == test_hash[i].condition) {
      /* not sure we will even get here */
      fprintf(where, "%s: g_cond_new error \n",__func__);
      fflush(where);
      exit(-2);
    }
#else
    test_hash[i].hash_lock = 
      (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));

    if (NULL == test_hash[i].hash_lock) {
      fprintf(where, "%s: unable to malloc a mutex \n",__func__);
      fflush(where);
      exit(-2);
    }

    rc = pthread_mutex_init(test_hash[i].hash_lock, NULL);
    if (rc) {
      fprintf(where, "%s: test pthread_mutex_init error %d\n", __func__, rc);
      fflush(where);
      exit(rc);
    }
    test_hash[i].condition = 
      (pthread_cond_t *)malloc(sizeof(pthread_cond_t));

    if (NULL == test_hash[i].condition) {
      fprintf(where, "%s: unable to malloc a pthread_cond_t \n",__func__);
      fflush(where);
      exit(-2);
    }

    rc = pthread_cond_init(test_hash[i].condition, NULL);
    if (rc) {
      fprintf(where, "%s: test pthread_cond_init error %d\n", __func__, rc);
      fflush(where);
      exit(rc);
    }
#endif
  }

  netlib_init();

}



int
main (int argc, char **argv)
{

  int sock;
  struct sockaddr name;
  netperf_socklen_t namelen = sizeof(name);

  program_name = argv[0];

#ifdef WITH_GLIB
  g_thread_init(NULL);
#endif

  /* if netserver is invoked with any of the -L, -p or -f options it
     will necessary to setup the listen endpoint.  similarly, if
     standard in is not a socket, it will be necessary to setup the
     listen endpoint.  otherwise, we assume we sprang-forth from inetd
     or the like and should start processing requests. */

  if (getsockname(0, &name, &namelen) == -1) {
      /* we may not be a child of inetd */
      if (CHECK_FOR_NOT_SOCKET) {
	need_setup = 1;
      }
  }

  decode_command_line(argc, argv);

  if (ofile) {
    /* send our stuff to wherever the user told us to */
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

  /* if netserver is invoked with any of the -L, -p or -f options it
     will necessary to setup the listen endpoint.  similarly, if
     standard in is not a socket, it will be necessary to setup the
     listen endpoint.  otherwise, we assume we sprang-forth from inetd
     or the like and should start processing requests. */

  if (getsockname(0, &name, &namelen) == -1) {
      /* we may not be a child of inetd */
      if (CHECK_FOR_NOT_SOCKET) {
	need_setup = 1;
      }
  }

  if (need_setup) {
    setup_listen_endpoint(listen_port);
  }
  else {
    sock = 0;
    handle_netperf_requests(sock);
  }
}

