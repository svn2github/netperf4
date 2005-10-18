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

#include <stdio.h>
#include <sys/types.h>

#ifndef HAVE_GETOPT_LONG
#include "missing/getopt.h"
#endif

#include <sys/socket.h>
#include <string.h>

#include <errno.h>

#include <netinet/in.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "netperf.h"
#include "netlib.h"

extern struct msgs NS_Msgs;

struct msgs *np_msg_handler_base = &NS_Msgs;

int debug =0;

/* gcc does not like intializing where here */
FILE *where;
xmlChar *my_nid=NULL;

#define NETPERF_HASH_BUCKETS 1

server_hash_t  netperf_hash[NETPERF_HASH_BUCKETS];
test_hash_t    test_hash[TEST_HASH_BUCKETS];

char *program_name;

/* Option flags and variables */

char      *oname = "stdout";                            /* --output */
FILE      *ofile;
int        want_quiet;                                  /* --quiet, --silent */
int        want_brief;                                  /* --brief */
int        want_verbose;                                /* --verbose */
char      *listen_port = NETPERF_DEFAULT_SERVICE_NAME;  /* --port */
uint16_t   listen_port_num = 0;                         /* --port */
char local_host_name[1024];
int  local_address_family = AF_UNSPEC;

char	arg1[BUFSIZ],  /* argument holders		*/
        arg2[BUFSIZ];

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
  ofile = stdout;


  while ((c = getopt_long (argc, argv,
			   "d"  /* debug */
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
	case 'd':               /* --debug */
	  debug++;
	  break;
	case 'L':
	  break_args_explicit(optarg,arg1,arg2);
	  if (arg1[0]) {
	    strncpy(local_host_name,arg1,sizeof(local_host_name));
	  }
	  if (arg2[0]) {
	    local_address_family = parse_address_family(arg2);
	    /* if only the address family was set, we may need to set the
	       local_host_name accordingly. since our defaults are IPv4
	       this should only be necessary if we have IPv6 support raj
	       2005-02-07 */  
#if defined (AF_INET6)
	    if (!arg1[0]) {
	      strncpy(local_host_name,"::0",sizeof(local_host_name));
	    }
#endif
	  }
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
        case 'p':               /* --output */
          /* we want to open a listen socket at a */
          /* specified port number */
          listen_port = strdup(optarg);
          listen_port_num = atoi(listen_port);
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
  pthread_mutex_lock(&(netperf_hash[hash_value].hash_lock));

  new_netperf->next = netperf_hash[hash_value].server;
  new_netperf->lock = &(netperf_hash[hash_value].hash_lock);
  netperf_hash[hash_value].server = new_netperf;

  pthread_mutex_unlock(&(netperf_hash[hash_value].hash_lock));

  return(NPE_SUCCESS);
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
  pthread_mutex_lock(&(netperf_hash[hash_value].hash_lock));

  server_pointer = netperf_hash[hash_value].server;
  while (server_pointer != NULL) {
    if (!xmlStrcmp(server_pointer->id,id)) {
      /* we have a match */
      break;
    }
    server_pointer = server_pointer->next;
  }

  pthread_mutex_unlock(&(netperf_hash[hash_value].hash_lock));

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

  if (debug) {
    fprintf(where,"instantiate_netperf: entered\n");
    fflush(where);
  }

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
                    "instantiate_netperf: Received an unexpected message\n");
            fflush(where);
          }
          rc = NPE_UNEXPECTED_MSG;
        } else {
          /* require the caller to ensure the netperf isn't already around */
          my_nid   = xmlStrdup(xmlGetProp(msg,(const xmlChar *)"tonid"));
          from_nid = xmlStrdup(xmlGetProp(msg,(const xmlChar *)"fromnid"));

          if ((netperf = (server_t *)malloc(sizeof(server_t))) == NULL) {
            fprintf(where,"instantiate_netperf: malloc failed\n");
            fflush(where);
            exit(1);
          }
          memset(netperf,0,sizeof(server_t));
          netperf->id        = from_nid;
          netperf->sock      = sock;
          netperf->state     = NSRV_CONNECTED;
          netperf->state_req = NSRV_WORK;
          netperf->tid       = -1;
          netperf->next      = NULL;

          /* check the version */
          rc = ns_version_check(cur,message,netperf);
        }
      }
    } else {
      if (debug) {
        fprintf(where,"instantiate_netperf: close connection remote close\n");
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
}



/* Netserver initialization */
static void
netserver_init()
{
  int   i;
  int   rc;
  int   sock;

  struct sockaddr  name;
  struct sockaddr *peeraddr     = &name;
  int              namelen      = sizeof(name);
  int              peerlen      = namelen;
  int              peeraddr_len = namelen;

  char *service = NULL;

  for (i = 0; i < NETPERF_HASH_BUCKETS; i++) {
    netperf_hash[i].server = NULL;
    rc = pthread_mutex_init(&(netperf_hash[i].hash_lock), NULL);
    if (rc) {
      fprintf(where, "netperf_init: pthread_mutex_init error %d\n",rc);
      fflush(where);
      exit;
    }
    rc = pthread_cond_init(&(netperf_hash[i].condition), NULL);
    if (rc) {
      fprintf(where, "netperf_init: pthread_cond_init error %d\n",rc);
      fflush(where);
      exit;
    }
  }
 
  for (i = 0; i < TEST_HASH_BUCKETS; i ++) {
    test_hash[i].test = NULL;
    rc = pthread_mutex_init(&(test_hash[i].hash_lock), NULL);
    if (rc) {
      fprintf(where, "netserver_init: pthread_mutex_init error %d\n",rc);
      fflush(where);
      exit;
    }
    rc = pthread_cond_init(&(test_hash[i].condition), NULL);
    if (rc) {
      fprintf(where, "server_init: pthread_cond_init error %d\n",rc);
      fflush(where);
      exit;
    }
  }

  netlib_init();

  int listenfd=0;
  /*
     if we were given a port number on the command line open a socket.
     Otherwise, if fd 0 is a socket then assume netserver was called by
     inetd and the socket and connection where created by inetd.
     Otherwise, use the default service name to open a socket.
   */

  if (listen_port_num==0) {
    if (getsockname(0, &name, &namelen) == -1) {
      /* we may not be a child of inetd */
#ifdef WIN32
      if (WSAGetLastError() == WSAENOTSOCK) {
        service=listen_port;
      }
#else
      if (errno == ENOTSOCK) {
        service=listen_port;
      }
#endif /* WIN32 */
    }
  } else {
    service=listen_port;
  }

  if (service != NULL) {
    /* we are not a child of inetd start a listen socket */
    listenfd = establish_listen(local_host_name,
				service,
				local_address_family,
				&peerlen);

    if (listenfd == -1) {
      fprintf(where,"netserver_init: failed to open listen socket\n");
      fflush(where);
      exit(1);
    }

    if (peerlen > namelen) {
      peeraddr = (struct sockaddr *)malloc (peerlen);
      peeraddr_len = peerlen;
    }

/* turn off fork for now so that netserver can be debuged in the foreground */
#ifdef OFF
    switch (fork()) {
      case -1:
        fprintf(where,"netserver_init: fork failed\n");
        fflush(where);
        exit(1);

      case 0:
        if (debug) {
          fprintf(where, "netserver_init: fork returned 0\n");
          fflush(where);
        }
        fclose(stdin);
        fclose(stderr);
        /* can I just use setsid on all systems? raj 4/96 */
        /* I choose to use only setsid sgb 8/03 */
        setsid();
        signal(SIGCLD, SIG_IGN);
#endif

        if ((sock = accept(listenfd,peeraddr,&peeraddr_len)) == -1) {
          fprintf(where,"netserver_init: accept failed\n");
          fflush(where);
          exit(1);
        }
        if (debug) {
          fprintf(where,
                  "netserver_init: accepted connection on sock %d\n",
                  sock);
          fflush(where);
        }

#ifdef OFF
        break;

      default:
        if (debug) {
          fprintf(where, "netserver_init: fork return !=0 and !=-1\n");
          fflush(where);
        }
        exit(0);
    }
#endif

  } else {
    /* we are a child of inetd */
    sock = 0;
  }
  
  rc = instantiate_netperf(sock);
  if (rc != NPE_SUCCESS) {
    fprintf(where, "netserver_init: instantiate_netperf  error %d\n",rc);
    fflush(where);
    exit;
  }
}


static void
check_test_state()
{
  int           i;
  int           orig;
  int           new;
  int           rc;
  test_t       *test;
  test_hash_t  *h;
  xmlNodePtr    msg = NULL;
  xmlNodePtr    new_node;
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
          fprintf(where,"check_test_state:tid = %s  state %d  new_state %d\n",
                  test->id, orig, new);
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
                      "check_test_state: send_control_message failed\n");
              fflush(where);
            }
          }
        }
      }
      test = test->next;
    }
    /* mutex unlocking is not required because only one 
       netserver thread looks at these data structures sgb */
  }
}


static void
handle_netperf_requests()
{
  int rc = NPE_SUCCESS;
  struct pollfd fds;
  xmlDocPtr     message;
  server_t     *netperf;

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
          fprintf(where,"process_message returned %d %s\n", rc);
          fflush(where);
        }
      } else {
        netperf->state = NSRV_ERROR;
        netperf->err_fn = "handle_netperf_requests";
        if (rc == 0) {
          netperf->err_rc = NPE_REMOTE_CLOSE;
        } else {
          fprintf(where,"recv_control_message returned %d\n",rc);
          fflush(where);
          netperf->err_rc = rc;
        }
      }
    } else {
      if (debug) {
        fprintf(where,"ho hum, nothing to do\n");
        fflush(where);
        report_test_status(netperf);
      }
    }
    /* mutex locking is not required because only one 
       netserver thread looks at these data structures sgb */
    if (rc != NPE_SUCCESS) {
      netperf->state  = NSRV_ERROR;
      netperf->err_rc = rc;
      netperf->err_fn = "handle_netperf_requests";
    }
  }

  if (rc != NPE_SUCCESS) {
    report_server_error(netperf);
  }
  /* mutex unlocking is not required because only one 
     netserver thread looks at these data structures sgb */
}


int
main (int argc, char **argv)
{
  int i;
  int sock;

  program_name = argv[0];

  where = stdout;

  i = decode_command_line(argc, argv);

  /* do the work */
  xmlInitParser();
  xmlKeepBlanksDefault(0);

  netserver_init();
  handle_netperf_requests();
}

