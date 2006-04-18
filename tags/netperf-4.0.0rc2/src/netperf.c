char netperf_id[]="\
@(#)netperf.c (c) Copyright 2005, Hewlett-Packard Company, $Id$";
/*
   netperf - network-oriented performance benchmarking

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

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

# ifdef HAVE_GLIB_H
#  include <glib.h>
# endif 

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
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

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifndef HAVE_GETOPT_LONG
#include "missing/getopt.h"
#else
#include <getopt.h>
#endif

/* #include "system.h" */

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "netperf.h"
#include "netlib.h"
#include "netmsg.h"

extern struct msgs NP_Msgs;

struct msgs *np_msg_handler_base = &NP_Msgs;

const xmlChar *my_nid = (const xmlChar *)"netperf";

int debug = 0;
/* gcc does not seem to like having where set to stderr in the declaration */
FILE *where;
xmlDocPtr config_doc;

server_hash_t server_hash[SERVER_HASH_BUCKETS];

test_hash_t test_hash[TEST_HASH_BUCKETS];

tset_hash_t test_set_hash[TEST_SET_HASH_BUCKETS];

xmlDocPtr config_doc = NULL;

char *program_name;

#define  GET_STATS        1
#define  CLEAR_STATS      2

/* getopt_long return codes */
enum {DUMMY_CODE=129
      ,BRIEF_CODE
};


/* Option flags and variables */

char *oname = "stdout";         /* --output */
FILE *ofile;
char *iname = NULL;             /* --input */
FILE *ifile;
char *cname = NULL;             /* configuration file */
int want_quiet;                 /* --quiet, --silent */
int want_brief;                 /* --brief */
int want_verbose;               /* --verbose */
int want_batch = 0;             /* set when -i specified */


/* forward declarations for local procedures */
static int process_command(xmlNodePtr cmd);
static int process_events();


static struct option const long_options[] =
{
  {"output", required_argument, 0, 'o'},
  {"input", required_argument, 0, 'i'},
  {"config", required_argument, 0, 'c'},
  {"debug", no_argument, 0, 'd'},
  {"quiet", no_argument, 0, 'q'},
  {"silent", no_argument, 0, 'q'},
  {"brief", no_argument, 0, BRIEF_CODE},
  {"verbose", no_argument, 0, 'v'},
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},
  {NULL, 0, NULL, 0}
};


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

gboolean set_input_source(gchar *option_name, gchar *option_value, gpointer data, GError **error) {

  iname = g_strdup(option_value);
  ifile = fopen(iname, "r");
  if (!ifile) {
    g_fprintf(where,
	      "%s: cannot open %s for reading\n",
	      program_name,
	      option_value);
    fflush(where);
    exit(1);
  } 
  else {
    want_batch = 1;
    fclose(ifile);
    ifile = NULL;
  }

  return(TRUE);
}

gboolean set_config_source(gchar *option_name, gchar *option_value, gpointer data, GError **error) {
  cname = g_strdup(option_value);
  return(TRUE);
}


static GOptionEntry netperf_entries[] = 
  {
    {"output",'o', 0, G_OPTION_ARG_CALLBACK, (void *)set_output_destination, "Specify where misc output should go", NULL},
    {"input",'i',0, G_OPTION_ARG_CALLBACK, (void *)set_input_source, "Specify the command file", NULL},
    {"config", 'c', 0, G_OPTION_ARG_CALLBACK, (void *)set_config_source, "Specify the configuration file", NULL},
    {"debug", 'd', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, (void *)set_debug, "Set the level of debugging output", NULL},
    {"version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, (void *)show_version, "Display the netserver version and exit", NULL},
    {"verbose", 'v', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, (void *)set_verbose, "Set verbosity level for output", NULL},
    {"brief", 'b', 0, G_OPTION_ARG_NONE, &want_brief, "Request brief output", NULL},
    {"quiet", 'q', 0, G_OPTION_ARG_NONE, &want_quiet, "Display no test banners", NULL},
    { NULL }
  };


static xmlNodePtr
parse_xml_file(char *fname,const xmlChar *doctype, xmlDocPtr *document)
{
  xmlDocPtr  doc = NULL;
  xmlNodePtr root = NULL;
  xmlNsPtr   ns;
  struct stat buf;

  if (debug) {
    if (fname) {
      fprintf(where,"%s called with fname %s\n",__func__,fname);
    }
    else {
      fprintf(where,"%s called with null fname\n",__func__);
    }
    fflush(where);
  }
  if (fname == NULL) {
    if (!xmlStrcmp(doctype,(const xmlChar *)"netperf")) {
      if (0 == g_stat("default_config.xml",&buf)) {
	fname = "default_config.xml";
      }
      else if (0 == g_stat(NETPERFDIR G_DIR_SEPARATOR_S "default_config.xml",
			 &buf)) {
	fname = NETPERFDIR G_DIR_SEPARATOR_S "default_config.xml";
      }
      else {
	fname = "missing config file";
      }
    }
    if (!xmlStrcmp(doctype,(const xmlChar *)"commands")) {
      if (0 == g_stat("default_commands.xml",&buf)) {
	fname = "default_commands.xml";
      }
      else if (0 == g_stat(NETPERFDIR G_DIR_SEPARATOR_S "default_commands.xml",
			 &buf)) {
	fname = NETPERFDIR G_DIR_SEPARATOR_S "default_commands.xml";
      }
      else {
	fname = "missing commands file";
      }
    }
  }

  if (debug) {
    fprintf(where,"%s parsing file %s\n",__func__,fname);
    fflush(where);
  }
  
  if ((doc = xmlParseFile(fname)) != NULL) {
    /* find the root element of the document */
    if ((root = xmlDocGetRootElement(doc)) != NULL) {
      /* now make sure that the netperf namespace is present */
      ns = xmlSearchNsByHref(doc, root,
                  (const xmlChar *)"http://www.netperf.org/ns/netperf");
      if (ns != NULL) {
        if (!xmlStrcmp(root->name,doctype)) {
          /* happy day valid document */
          *document = doc;
        } else {
          /* not a correct document type*/
          fprintf(where,
                  "file %s is of type \"%s\" not \"%s\"\n",
                  fname,root->name,doctype);
          fflush(where);
          xmlFreeDoc(doc);
          doc  = NULL;
          root = NULL;
        }
      } else {
        /* no namespace match */
        fprintf(where,
                "file %s does not reference a netperf namespace\n",fname);
        fflush(where);
        xmlFreeDoc(doc);
        doc  = NULL;
        root = NULL;
      }
    } else {
      /* empty document */
      fprintf(where,
              "file %s contains no root element\n",fname);
      xmlFreeDoc(doc);
      fflush(where);
      doc = NULL;
    }
  } else {
    fprintf(where,
            "file %s contained invalid XML\n",fname);
    fflush(where);
  }
  return(root);
}


/* Netperf initialization */
static void
netperf_init()
{
  int i;


  for (i = 0; i < SERVER_HASH_BUCKETS; i++) {
    server_hash[i].server = NULL;
    server_hash[i].hash_lock = g_mutex_new();
    if (NULL == server_hash[i].hash_lock) {
      /* not sure we will even get here */
      fprintf(where, "%s: g_cond_new error \n",__func__);
      fflush(where);
      exit(-2);
    }
    server_hash[i].condition = g_cond_new();
    if (NULL == server_hash[i].condition) {
      /* not sure we will even get here */
      fprintf(where, "netperf_init: g_cond_new error \n");
      fflush(where);
      exit(-2);
    }
  }

  for (i = 0; i < TEST_HASH_BUCKETS; i ++) {
    test_hash[i].test = NULL;
    test_hash[i].hash_lock = g_mutex_new();
    if (NULL == test_hash[i].hash_lock) {
      /* not sure we will even get here */
      fprintf(where, "%s: g_mutex_new error \n",__func__);
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
  }

  /* you know, it might not be a bad idea to also properly initialize
     the test_set_hash stuff too, nudge nudge to you know who!  */

  for (i = 0; i < TEST_SET_HASH_BUCKETS; i ++) {
    test_set_hash[i].test_set = NULL;
    test_set_hash[i].hash_lock = g_mutex_new();
    if (NULL == test_set_hash[i].hash_lock) {
      /* not sure we will even get here */
      fprintf(where, "%s: g_mutex_new error \n",__func__);
      fflush(where);
      exit(-2);
    }
    test_set_hash[i].condition = g_cond_new();
    if (NULL == test_set_hash[i].condition) {
      /* not sure we will even get here */
      fprintf(where, "%s: g_cond_new error \n",__func__);
      fflush(where);
      exit(-2);
    }
  }

  netlib_init();
}



static void
display_server_hash()
{
  int i;
  server_t * server;
  for (i=0;i < SERVER_HASH_BUCKETS; i++) {
    server = server_hash[i].server;
    fprintf(where,"server_hash_bucket[%d]=%p\n",i,server_hash[i].server);
    while (server) {
      fprintf(where,"\tserver->id %s, server->channel %p, server->state %d\n",
              server->id,server->source,server->state);
      server = server->next;
    }
    fflush(where);
  }
}

static int
add_server_to_hash(server_t *new_server) {

  int hash_value;

  hash_value = ((atoi((char *)new_server->id + 1)) % SERVER_HASH_BUCKETS);

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(server_hash[hash_value].hash_lock);

  new_server->next = server_hash[hash_value].server;
  new_server->lock = server_hash[hash_value].hash_lock;
  server_hash[hash_value].server = new_server;

  NETPERF_MUTEX_UNLOCK(server_hash[hash_value].hash_lock);

  return(NPE_SUCCESS);
}

static void
delete_server(const xmlChar *id)
{
  /* we presume that the id is of the form [a-zA-Z][0-9]+ and so will
     call atoi on id and mod that with the SERVER_HASH_BUCKETS */

  int       hash_value;
  server_t *server_pointer;
  server_t **prev_server;


  hash_value = ((atoi((char *)id + 1)) % SERVER_HASH_BUCKETS);

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(server_hash[hash_value].hash_lock);

  prev_server    = &(server_hash[hash_value].server);
  server_pointer = server_hash[hash_value].server;
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

  NETPERF_MUTEX_UNLOCK(server_hash[hash_value].hash_lock);

}


static server_t *
find_server_in_hash(const xmlChar *id) {

  /* we presume that the id is of the form [a-zA-Z][0-9]+ and so will
     call atoi on id and mod that with the SERVER_HASH_BUCKETS */

  int hash_value;
  server_t *server_pointer;

  hash_value = ((atoi((char *)id + 1)) % SERVER_HASH_BUCKETS);

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(server_hash[hash_value].hash_lock);

  server_pointer = server_hash[hash_value].server;
  while (server_pointer != NULL) {
    if (!xmlStrcmp(server_pointer->id,id)) {
      /* we have a match */
      break;
    }
    server_pointer = server_pointer->next;
  }

  NETPERF_MUTEX_UNLOCK(server_hash[hash_value].hash_lock);

  return(server_pointer);
}


static int
add_test_set_to_hash(tset_t *new_test_set)
{
  int hash_value;

  hash_value = TEST_SET_HASH_VALUE(new_test_set->id);

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(test_set_hash[hash_value].hash_lock);

  new_test_set->next = test_set_hash[hash_value].test_set;

  test_set_hash[hash_value].test_set = new_test_set;

  NETPERF_MUTEX_UNLOCK(test_set_hash[hash_value].hash_lock);

  return(NPE_SUCCESS);
}

static int
delete_test_set_from_hash(const xmlChar *id)
{
  /* we presume that the id is of the form [s][0-9]+ and so will
     call atoi on id and mod that with the TEST_SET_HASH_BUCKETS */

  int hash_value;
  tset_t *test_set_ptr;
  tset_t *prev = NULL;

  hash_value = TEST_SET_HASH_VALUE(id);

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(test_set_hash[hash_value].hash_lock);

  test_set_ptr = test_set_hash[hash_value].test_set;
  while (test_set_ptr != NULL) {
    if (!xmlStrcmp(test_set_ptr->id,id)) {
      /* we have a match */
      break;
    }
    prev         = test_set_ptr;
    test_set_ptr = test_set_ptr->next;
  }

  if (test_set_ptr) {
    if (prev) {
      prev->next = test_set_ptr->next;
    } else {
      test_set_hash[hash_value].test_set = test_set_ptr->next;
    }
  }

  NETPERF_MUTEX_UNLOCK(test_set_hash[hash_value].hash_lock);

  return(NPE_SUCCESS);
}

static tset_t *
find_test_set_in_hash(const xmlChar *id)
{
  /* we presume that the id is of the form [s][0-9]+ and so will
     call atoi on id and mod that with the TEST_SET_HASH_BUCKETS */

  int hash_value;
  tset_t *test_set_ptr;

  hash_value = TEST_SET_HASH_VALUE(id);

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(test_set_hash[hash_value].hash_lock);

  test_set_ptr = test_set_hash[hash_value].test_set;
  while (test_set_ptr != NULL) {
    if (!xmlStrcmp(test_set_ptr->id,id)) {
      /* we have a match */
      break;
    }
    test_set_ptr = test_set_ptr->next;
  }

  NETPERF_MUTEX_UNLOCK(test_set_hash[hash_value].hash_lock);
  return(test_set_ptr);
}


static SOCKET
connect_netserver(xmlDocPtr doc, xmlNodePtr netserver, server_t *new_server)
{
  xmlChar   *remotehost;
  xmlChar   *remoteport;
  xmlChar   *localhost;
  xmlChar   *localport;

  int      localfamily;
  int      remotefamily;
  SOCKET      sock;

  /* validation means that we know that the host informaion is in attributes
     of the netserver element. The xml parser checked them and initialized
     any optional attributes which where not present.  sgb 2003-10-11 */

  /* pull the remote address family, service and host attributes */
  remotefamily = strtofam(xmlGetProp(netserver,(const xmlChar *)"family"));
  remoteport   = xmlGetProp(netserver,(const xmlChar *)"remote_service");
  remotehost   = xmlGetProp(netserver,(const xmlChar *)"remote_host");

  /* for the time being, the netserver element definition provides only
     one family attribute - this means no mixed family stuff, which should
     not be a problem for the moment.  since getaddrinfo() takes a servname
     parm as an ASCII string we will just pass the string. sgb 2003-10-11 */

  /* now pull the local address family, service and host attributes */
  localfamily  = strtofam(xmlGetProp(netserver,(const xmlChar *)"family"));
  localport    = xmlGetProp(netserver,(const xmlChar *)"local_service");
  localhost    = xmlGetProp(netserver,(const xmlChar *)"local_host");

  /* and now call establish_control */
  sock = establish_control(remotehost, remoteport, remotefamily,
                           localhost,  localport,  localfamily);
  return(sock);
}


static int
instantiate_tests(xmlNodePtr server_node, server_t *server )
{
  int        rc = NPE_SUCCESS;
  test_t    *new_test;
  xmlNodePtr this_test;
  xmlChar   *testid;
  xmlChar   *serverid = xmlGetProp(server_node,(xmlChar *)"nid");

  /* Now get the first child node, all children are test elements or
     comments and all test elements have a unique tid value.  The check
     for correctness was done by the xml parser.  sbg 2003-10-02 */
  this_test = server_node->xmlChildrenNode;
  while (this_test != NULL && rc == NPE_SUCCESS) {
    if (xmlStrcmp(this_test->name,(const xmlChar *)"test")) {
      if (debug) {
        printf("instantiate_tests skipped a non-test node\n");
      }
      this_test = this_test->next;
      continue;
    }
    testid = xmlGetProp(this_test,(const xmlChar *)"tid");
    new_test = (test_t *)malloc(sizeof(test_t));
    if (new_test != NULL) { /* we have a new test_t structure */
      memset(new_test,0,sizeof(test_t));
      new_test->node      = this_test;
      new_test->id        = testid;
      new_test->server_id = serverid;
      new_test->state     = TEST_PREINIT;
      new_test->state_req = TEST_PREINIT;
      rc = get_test_function(new_test,(const xmlChar *)"test_name");
      if (rc == NPE_SUCCESS) {
        rc = get_test_function(new_test,(const xmlChar *)"test_decode");
      }
    } else {
      rc = NPE_MALLOC_FAILED3;
    }
    if (rc == NPE_SUCCESS) {
      rc = add_test_to_hash(new_test);
    }
    if (debug) {
      fprintf(where, "instantiate_tests testid='%s' returned %d\n",testid,rc);
    }
    this_test = this_test->next;
  }
  return(rc);
}


static void
instantiate_netservers()
{
  int         rc = NPE_SUCCESS;
  xmlNodePtr  netperf;
  xmlNodePtr  this_netserver;
  server_t   *new_server;
  xmlChar    *netserverid;
  GIOStatus   status;
  GError      *error = NULL;

  /* first, get the netperf element which was checked at parse time.  The
     netperf element has only netserver elements each with its own unique
     nid ID attribute. Duplicates should have been caught by the xml parser.
     Users must correctly setup configuration files. Syntax errors in the
     netperf configuration file should have been caught by the xml parser
     when it did a syntax check against netperf_docs.dtd sgb 2003-10-7 */

  netperf = xmlDocGetRootElement(config_doc);

  /* now get the first netserver node */
  this_netserver = netperf->xmlChildrenNode;

  while (this_netserver != NULL && rc == NPE_SUCCESS) {
    if (xmlStrcmp(this_netserver->name,(const xmlChar *)"netserver")) {
      if (debug) {
        printf("instantiate_netservers skipped a non-netserver node\n");
      }
      this_netserver = this_netserver->next;
      continue;
    }
    netserverid = xmlGetProp(this_netserver,(const xmlChar *)"nid");
    new_server = (server_t *)malloc(sizeof(server_t));
    if (new_server != NULL) {    /* we have a new server_t structure */
      memset(new_server,0,sizeof(server_t));
      new_server->id        = netserverid;
      if (add_server_to_hash(new_server) == NPE_SUCCESS) {
        new_server->node      = this_netserver;
	g_static_rw_lock_init(&new_server->rwlock);
        if (rc == NPE_SUCCESS) {
          rc = instantiate_tests(this_netserver, new_server);
        }
        if (rc == NPE_SUCCESS) { /* instantiate_tests was successful */
            new_server->sock      = connect_netserver(config_doc,
                                                      this_netserver,
                                                      new_server);
            if (new_server->sock == -1) {
              /* connect netserver unhappy */
              fprintf(where,"connect_netserver: Failed\n");
              fflush(where);
              new_server->state     = NSRV_PREINIT;
              new_server->state_req = NSRV_PREINIT;
              rc = NPE_CONNECT_FAILED;
            }
#ifdef G_OS_WIN32
	    new_server->source = g_io_channel_win32_new_socket(new_server->sock);
#else
	    new_server->source = g_io_channel_unix_new(new_server->sock);
#endif
	    status = g_io_channel_set_flags(new_server->source,
					    G_IO_FLAG_NONBLOCK,
					    &error);
	    if (error) {
	      g_warning("g_io_channel_set_flags %s %d %s\n",
			g_quark_to_string(error->domain),
			error->code,
			error->message);
	      g_clear_error(&error);
	    }
    
	    status = g_io_channel_set_encoding(new_server->source,NULL,&error);
	    if (error) {
	      g_warning("g_io_channel_set_encoding %s %d %s\n",
			g_quark_to_string(error->domain),
			error->code,
			error->message);
	      g_clear_error(&error);
	    }
	    
	    g_io_channel_set_buffered(new_server->source,FALSE);

            new_server->state     = NSRV_CONNECTED;
            new_server->state_req = NSRV_CONNECTED;
          rc = send_version_message(new_server, my_nid);
          if (rc == NPE_SUCCESS) {
            new_server->state     = NSRV_VERS;
            new_server->state_req = NSRV_INIT;
          }
        }
      } else {  /* add_server_to_hash  failed */
        rc = NPE_SHASH_ADD_FAILED;
      }
    } else { /* unhappy malloc */
      rc = NPE_MALLOC_FAILED4;
    }
    if (debug) {
      fprintf(where, "instantiate_netservers: netserver nid='%s' returned %d\n",
              netserverid,rc);
      fflush(where);
    }
    this_netserver = this_netserver->next;
  }
  if (rc != NPE_SUCCESS) {
    exit(-1);
  }
}


static int
wait_for_version_response(server_t *server)
{
  int        rc = NPE_SUCCESS;
  fd_set     read_fds;
  fd_set     error_fds;
  struct timeval timeout;

  xmlDocPtr  message;

  if (debug) {
    fprintf(where,"entering wait_for_version_response\n");
    fflush(where);
  }
  NETPERF_MUTEX_LOCK(server->lock);
  while (server->state == NSRV_VERS) {
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    FD_ZERO(&read_fds);
    FD_ZERO(&error_fds);
    FD_SET(server->sock,&read_fds);
    FD_SET(server->sock,&error_fds);

    NETPERF_MUTEX_UNLOCK(server->lock);
    if (select(server->sock + 1,
	       &read_fds,
	       NULL,
	       &error_fds,
	       &timeout) > 0) {
      if (debug) {
        fprintf(where,"wait_for_version_response ");
        fprintf(where,"calling recv_control_message\n");
        fflush(where);
      }
      rc = recv_control_message(server->sock, &message);
      if (rc > 0) {
        if (debug) {
          fprintf(where,"wait_for_version_response ");
          fprintf(where,"calling process_message\n");
          fflush(where);
        }
        rc = process_message(server, message);
      } else {
        NETPERF_MUTEX_LOCK(server->lock);
        server->state  = NSRV_ERROR;
        server->err_fn = (char *)__func__;
        if (rc == 0) {
          server->err_rc = NPE_REMOTE_CLOSE;
        } else {
          server->err_rc = rc;
        }
        NETPERF_MUTEX_UNLOCK(server->lock);
      }
    } else {
      if (debug) {
        fprintf(where,"ho hum, waiting for netserver version response\n");
        fflush(where);
      }
    }
    NETPERF_MUTEX_LOCK(server->lock);
    if (rc == NPE_SUCCESS) {
      /* accepted version string move netserver to NSRV_INIT state */
      server->state     = server->state_req;
      server->state_req = NSRV_WORK;
    } else {
      server->state  = NSRV_ERROR;
      server->err_fn = (char *)__func__;
      server->err_rc = rc;
    }
  }

  if (rc != NPE_SUCCESS) {
    report_server_error(server);
  }
  NETPERF_MUTEX_UNLOCK(server->lock);

  NETPERF_DEBUG_EXIT(debug,where);

  return rc;
}


static void *initialize_test(void *data);

static int
resolve_dependency(xmlChar *id, xmlNodePtr *data)
{
  int              rc = NPE_DEPENDENCY_NOT_PRESENT;
  int              hash_value;
  test_t          *test;
  test_hash_t     *h;
  NETPERF_ABS_TIMESPEC  delta_time;
  NETPERF_ABS_TIMESPEC  abstime;

  NETPERF_ABS_TIMESET(delta_time,1,0);
  
  *data = NULL;

   
  /* find the dependency in the hash list */
  hash_value = TEST_HASH_VALUE(id);
  h = &(test_hash[hash_value]);
  NETPERF_MUTEX_LOCK(h->hash_lock);
  test = h->test;
  while (test != NULL) {
    if (!xmlStrcmp(test->id,id)) {

#ifdef OFF
      /* found the test we depend on check its state */
      if (test->state_req == TEST_PREINIT) {
        /* test is not yet initialized initialize it */
        test->state_req = TEST_IDLE;
        NETPERF_MUTEX_UNLOCK(&h->hash_lock);

        rc = launch_thread(&test->tid, initialize_test, test);

        NETPERF_MUTEX_LOCK(&h->hash_lock);
        if (rc != NPE_SUCCESS) {
          test->state  = TEST_ERROR;
          test->err_rc = rc;
          test->err_fn = (char *)__func__;
          rc = NPE_DEPENDENCY_NOT_PRESENT;
          break;
        } 
      } /* end of test initilization */
#endif
      /* wait for test to initialize */
      while (test->state == TEST_PREINIT) {
        NETPERF_MUTEX_UNLOCK(h->hash_lock);
        if (debug > 1) {
          fprintf(where,
                  "resolve_dependency: waiting on test %s\n",
                  (char *)id);
          fflush(where);
        }

        NETPERF_MUTEX_LOCK(h->hash_lock);

	g_get_current_time(&abstime);
	g_time_val_add(&abstime,1000);
	g_cond_timed_wait(h->condition, h->hash_lock, &abstime);
      }  /* end wait */
      
      /* test has reached at least the TEST_INIT state */
      if (test->state != TEST_ERROR) {
        if (test->dependent_data != NULL) {
          *data = test->dependent_data;
          NETPERF_MUTEX_UNLOCK(h->hash_lock);
          rc = NPE_SUCCESS;
          if (debug) {
            fprintf(where,"resolve_dependency: successful for %s\n",
                    (char *)id);
            fflush(where);
          }
          NETPERF_MUTEX_LOCK(h->hash_lock);
        } else {
          rc = NPE_DEPENDENCY_NOT_PRESENT;
        }
      } else {
        rc = NPE_DEPENDENCY_NOT_PRESENT;
      }
      break;
    }

    test = test->next;
  }
  NETPERF_MUTEX_UNLOCK(test_hash[hash_value].hash_lock);
  return(rc);
}


static void *
initialize_test(void *data)
{
  test_t     *test            = data;
  xmlChar    *id              = NULL;
  xmlNodePtr  dep_data        = NULL;
  int         rc              = NPE_SUCCESS;
  xmlNodePtr  cur             = NULL;
  xmlNodePtr  msg             = NULL;
  server_t   *server;
  
  if (debug) {
    fprintf(where,"entering initialize_test\n");
    fflush(where);
  }

  cur = test->node->xmlChildrenNode;
  while (cur != NULL) {
    if (!xmlStrcmp(cur->name,(const xmlChar *)"dependson")) {
      if (debug) {
        printf("initialize_test found a dependson node\n");
      }
      id  = xmlGetProp(cur, (const xmlChar *)"testid");
      rc = resolve_dependency(id, &dep_data);
      break;
    }
    cur = cur->next;
  }

  server = find_server_in_hash(test->server_id);

  if (rc == NPE_SUCCESS) {
    if (debug) {
      /* there may not have been an actual dependency, which means
	 that 'id' (which used to be uninitialized, tsk, tsk, tsk) may
	 be NULL */
      fprintf(where,
              "%s: %s's dependency on %s successfully resolved\n",
              __func__,
	      test->id,
	      (id == NULL ? "none" : (char *)id));
      fflush(where);
    }
    /* any dependency has been successfully resolved
       now build the test message to send to the netserver */
    if ((msg = xmlCopyNode(test->node,1)) != NULL) {
      if (dep_data != NULL) {
        cur = msg->xmlChildrenNode;
        while (cur != NULL) {
          /* prune dependson node and replace with dependency data */
          if (!xmlStrcmp(cur->name,(const xmlChar *)"dependson")) {
            xmlReplaceNode(cur,dep_data);
            break;
          }
          cur = cur->next;
        }  
      }
      /* is the lock around the send required? */
      NETPERF_RWLOCK_WRLOCK(&server->rwlock);
      rc = write_to_control_connection(server->source,
				       msg,
				       server->id,
				       my_nid);
      NETPERF_RWLOCK_WRITER_UNLOCK(&server->rwlock);
    } else {
      if (debug) {
        fprintf(where,
                "%s: %s xmlCopyNode failed\n",
                __func__, test->id);
        fflush(where);
      }
      rc = NPE_INIT_TEST_XMLCOPYNODE_FAILED;
    }
  }

  if (rc != NPE_SUCCESS) {
    test->state  = TEST_ERROR;
    test->err_rc = rc;
    test->err_fn = (char *)__func__;
  }


  NETPERF_DEBUG_EXIT(debug,where);

  if (msg) xmlFreeNode(msg);

  return(test);
}


static void
initialize_tests(server_t *server)
{
  int          rc = NPE_SUCCESS;
  int          i;
  test_t      *test;
  test_hash_t *h;

  if (debug) {
    fprintf(where,"entering initialize_tests\n");
    fflush(where);
  }

  display_test_hash();

  for (i = 0; i < TEST_HASH_BUCKETS; i ++) {
    h = &test_hash[i];
    NETPERF_MUTEX_LOCK(h->hash_lock);
    test = h->test;
    while (test != NULL) {
      if (test->state_req == TEST_PREINIT) {
        if (!xmlStrcmp(server->id, test->server_id)) {
          /* test is not initialized and belongs to this netserver init it */
          test->state_req = TEST_IDLE;
          NETPERF_MUTEX_UNLOCK(h->hash_lock);
          rc = launch_thread(&test->thread_id, initialize_test, test);
          NETPERF_MUTEX_LOCK(h->hash_lock);
          if (rc != NPE_SUCCESS) {
            test->state = TEST_ERROR;
            test->err_rc = rc;
            test->err_fn = (char *)__func__;
          };
        }
        /* should we restart the chain just incase an entry was inserted
           or deleted?  no for now   sgb 2005-10-25 */
        /* test = h->test; */
        test = test->next;
      } else {
        /* test initialization has already been requested try next test */
        test = test->next;
      }
    }
    NETPERF_MUTEX_UNLOCK(h->hash_lock);
  }

  NETPERF_DEBUG_EXIT(debug,where);

}


static void *
netperf_worker(void *data)
{
  int rc;
  server_t     *server = data;
  fd_set read_fds;
  fd_set error_fds;
  struct timeval timeout;

  xmlDocPtr     message;
  
  rc = wait_for_version_response(server);
  if (rc == NPE_SUCCESS) {
    initialize_tests(server);
  }
  NETPERF_MUTEX_LOCK(server->lock);

  while (server->state != NSRV_ERROR) {
    FD_ZERO(&read_fds);
    FD_ZERO(&error_fds);
    FD_SET(server->sock,&read_fds);
    FD_SET(server->sock,&error_fds);
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    NETPERF_MUTEX_UNLOCK(server->lock);
    if (select(server->sock + 1,
	       &read_fds,
	       NULL,
	       &error_fds,
	       &timeout) > 0) {
      rc = recv_control_message(server->sock, &message);
      if (rc > 0) {
        rc = process_message(server, message);
      } else {
        if (rc == 0) {
          fprintf(where,"netperf_worker: got an return of 0 from %s\n",
                  "recv_control_message");
          fflush(where);
          rc = NPE_REMOTE_CLOSE;
        }
      }
    } else {
      if (debug) {
        fprintf(where,"netperf_worker: ho hum, nothing to do\n");
        fflush(where);
        report_servers_test_status(server);
      }
    }
    NETPERF_MUTEX_LOCK(server->lock);
    if (rc != NPE_SUCCESS) {
      server->state  = NSRV_ERROR;
      server->err_rc = rc;
      server->err_fn = (char *)__func__;
    }
  }

  if (rc != NPE_SUCCESS) {
    report_server_error(server);
  }
  NETPERF_MUTEX_UNLOCK(server->lock);

  return(server);
}


static void
launch_worker_threads()
{
 
  int            i;
  server_hash_t *h;
  server_t      *server;
  int            rc;

  if (debug) {
    fprintf(where,"entering launch_worker_treads\n");
    fflush(where);
  }
  for (i = 0; i < SERVER_HASH_BUCKETS; i ++) {
    h = &server_hash[i];
    NETPERF_MUTEX_LOCK(h->hash_lock);
    server = h->server;
    while (server != NULL) {
      if (server->state_req == NSRV_INIT) {
        /* netserver worker thread needs to be started */
        NETPERF_MUTEX_UNLOCK(h->hash_lock);
        if (debug) {
          fprintf(where,"launching thread for netserver %s\n",server->id);
          fflush(where);
        }
        /* netserver worker thread is not yet initialized start it */
        rc = launch_thread(&server->thread_id, netperf_worker, server);
        if (debug) {
          fprintf(where,
		  "launched thread for netserver %s\n",
		  server->id);
          fflush(where);
        }
        NETPERF_MUTEX_LOCK(h->hash_lock);
        if (rc != NPE_SUCCESS) {
          server->state = NSRV_ERROR;
          server->err_rc = rc;
          server->err_fn = (char *)__func__;
        }
      }
      server = server->next;
    }
    NETPERF_MUTEX_UNLOCK(h->hash_lock);
  }

  NETPERF_DEBUG_EXIT(debug,where);

}


static int
wait_for_tests_to_initialize()
{
  int              rc = NPE_SUCCESS;

  int              i;
  server_t        *server;
  test_t          *test;
  test_hash_t     *h;
  NETPERF_ABS_TIMESPEC delta_time;
  NETPERF_ABS_TIMESPEC  abstime;

  NETPERF_ABS_TIMESET(delta_time,1,0);

  if (debug) {
    fprintf(where,"entering wait_for_tests_to_initialize\n");
    fflush(where);
  }
  for (i = 0; i < TEST_HASH_BUCKETS; i ++) {
    h = &test_hash[i];
    NETPERF_MUTEX_LOCK(h->hash_lock);
    test = h->test;
    while (test != NULL) {
      while (test->state != TEST_IDLE) {
        if (test->state == TEST_ERROR) {
          rc = NPE_TEST_FOUND_IN_ERROR_STATE;
	  fprintf(where,
		  "%s found test %s %s in the error state\n",
		  __func__,
		  test->id,
		  test->test_name);
	  fprintf(where,
                  "err_str='%s'\n",
                  test->err_str);
	  fprintf(where,
                  "err_no=%d\t%s\n",
                  test->err_no,
                  npe_to_str(test->err_no));
                 
	  fflush(where);
          break;
        }
        /* test is not yet initialized wait for it */
	g_get_current_time(&abstime);
	g_time_val_add(&abstime,1000);
	g_cond_timed_wait(h->condition, h->hash_lock, &abstime);
        /* since the mutex was unlocked during the conditional wait
           should the hash chain be restarted in case a new test was
           inserted ? */
        /* test = h->test;  for now no */
      } 
      test = test->next;
    }
    NETPERF_MUTEX_UNLOCK(h->hash_lock);
  }
  for (i=0;i < SERVER_HASH_BUCKETS; i++) {
    server = server_hash[i].server;
    while (server) {
      /* set the netserver state to NSRV_WORK now are ready to run tests */
      NETPERF_MUTEX_LOCK(server->lock);

      server->state = server->state_req;

      NETPERF_MUTEX_UNLOCK(server->lock);

      server = server->next;
    }
  }
  if (debug) {
    fprintf(where,
	    "exiting wait_for_tests_to_initialize rc = %d %s\n",
	    rc,
	    npe_to_str(rc));
    fflush(where);
  }

  return(rc);
}


static void
wait_for_tests_to_enter_requested_state(xmlNodePtr cmd)
{
  int          rc = NPE_SUCCESS;
  xmlChar     *tid;
  test_t      *test;
  tset_t      *test_set;
  int          i;

  if (debug) {
    fprintf(where,"entering %s\n", __func__);
    fflush(where);
  }
  tid = xmlGetProp(cmd, (const xmlChar *)"tid");
  test_set = find_test_set_in_hash(tid);
  if (test_set) {
    tset_elt_t *set_elt = test_set->tests;
    while (set_elt) {
      test = set_elt->test;
      if (test) {
        i = 15;
        while (test->state != test->state_req) {
          if (test->state == TEST_ERROR) {
            rc = NPE_TEST_FOUND_IN_ERROR_STATE;
            break;
          }
          if (i) {
            i--;
            g_usleep(1000000);
          }
          else {
            i = 15;
            report_test_status(test);
          }
        }
      }
      set_elt = set_elt->next;
    }
  }
  else {
    test = find_test_in_hash(tid);
    if (test) {
      i = 15;
      while (test->state != test->state_req) {
        if (test->state == TEST_ERROR) {
          rc = NPE_TEST_FOUND_IN_ERROR_STATE;
          break;
        }
        if (i) {
          i--;
          g_usleep(1000000);
        }
        else {
          i = 15;
          report_test_status(test);
        }
      }
    }
  }
  if (debug) {
    fprintf(where,"exiting %s rc = %d\n", __func__, rc);
    fflush(where);
  }
}



static int
request_state_change(xmlNodePtr cmd, uint32_t state)
{
  int          rc   = NPE_SUCCESS;
  xmlChar     *tid;
  test_t      *test;
  server_t    *server;
  tset_t      *test_set;

  if (debug) {
    fprintf(where,"request_state_change: entered\n");
    fflush(where);
  }
  tid = xmlGetProp(cmd,(const xmlChar *)"tid");
  test_set = find_test_set_in_hash(tid);
  if (test_set) {
    tset_elt_t *set_elt = test_set->tests;
    while (set_elt) {
      test = set_elt->test;
      if (test) {
        if (debug) {
          fprintf(where,"test_id = '%s'  server_id ='%s'\n",
                  test->id,test->server_id);
          fflush(where);
        }
        server = find_server_in_hash(test->server_id);
        test->state_req = state;
        xmlSetProp(cmd,(const xmlChar *)"tid", test->id);
        rc = write_to_control_connection(server->source,
					 cmd,
					 server->id,
					 my_nid);
        if (rc != NPE_SUCCESS) {
          test->state  = TEST_ERROR;
          test->err_rc = rc;
          test->err_fn = (char *)__func__;
        }
      }
      set_elt = set_elt->next;
    }
    xmlSetProp(cmd,(const xmlChar *)"tid", test_set->id);
  } else {
    test = find_test_in_hash(tid);
    if (test) {
      if (debug) {
        fprintf(where,"test_id = '%s'  server_id ='%s'\n",
                test->id,test->server_id);
        fflush(where);
      }
      server = find_server_in_hash(test->server_id);
      test->state_req = state;
      xmlSetProp(cmd,(const xmlChar *)"tid", test->id);
      rc = write_to_control_connection(server->source,
				       cmd,
				       server->id,
				       my_nid);
      if (rc != NPE_SUCCESS) {
        test->state  = TEST_ERROR;
        test->err_rc = rc;
        test->err_fn = (char *)__func__;
      }
    }
  }

  NETPERF_DEBUG_EXIT(debug,where);

  return(rc);
}



static void
report_cmd_error(int rc)
{
  fprintf(where,"report_cmd_error: reporting command error = %d\n",rc);
  fflush(where);
}


static void
report_event_error(int rc)
{
  fprintf(where,"report_event_error: routine needs to be completed\n");
  fflush(where);
}


static int
report_stats_command(xmlNodePtr my_cmd, uint32_t junk)
{
  int            rc   = NPE_SUCCESS;
  xmlNodePtr     commands;
  xmlNodePtr     cmd;
  char          *output_file;
  char          *report_flags;
  xmlChar       *set_name;
  xmlChar       *string;
  char          *desired_level;
  char          *dsrd_interval;
  tset_t        *test_set;
  int            min_count;
  int            max_count;
  int            sample_count = 0;
  GenReport      gen_report   = NULL;
  
  commands      = my_cmd->xmlChildrenNode;

  if (debug) {
    fprintf(where,"report_stats_command: entered commands = %p\n",commands);
    fflush(where);
  }

  set_name      = xmlGetProp(my_cmd,(const xmlChar *)"test_set");
  report_flags  = (char *)xmlGetProp(my_cmd,(const xmlChar *)"report_flags");

  output_file   = (char *)xmlGetProp(my_cmd,(const xmlChar *)"output_file");

  /* once more, we have to make sure that there really is a valid
     string returned from the xmlGetProp call!!!  raj 2005-10-27 */
  string = xmlGetProp(my_cmd,(const xmlChar *)"max_count");
  if (string) {
    max_count     = atoi((char *)string);
  }
  else {
    max_count = 1;
  }
  string = xmlGetProp(my_cmd,(const xmlChar *)"min_count");
  if (string) {
    min_count     = atoi((char *)string);
  }
  else {
    min_count = 1;
  }

  if (commands == NULL) {
    max_count = 1;
    min_count = 1;
  }

  test_set = find_test_set_in_hash(set_name);
  if (debug) {
    fprintf(where,
            "report_stats_command: found test_set = '%p'\n",
            test_set);
    fprintf(where,
            "report_stats_command: max_count = %d   min_count = %d\n",
            max_count, min_count);
    fflush(where);
  }
  
  if (test_set) {
    if (debug) {
      fprintf(where,
              "report_stats_command: test_set = '%s'\n",
              test_set->id);
      fflush(where);
    }
    test_set->confidence.max_count = max_count;
    test_set->confidence.min_count = min_count;
    test_set->confidence.value     = -100.0;
    test_set->report_data          = NULL;
    test_set->debug                = debug;
    test_set->where                = where;
    /* The report function is responsible for the allocation and clean up
       of any report_data structures that are required across the multiple
       invocations that occur during the loop.  */

    fflush(where);
    if (min_count > 1) {
      fprintf(where, 
              "!!! TEST RUN USING CONFIDENCE  max_count = %d  min_count = %d\n",
              max_count, min_count);
      /* initialize confidence information */
      desired_level = (char*)(xmlGetProp(my_cmd,(const xmlChar *)"confidence"));
      dsrd_interval = (char*)(xmlGetProp(my_cmd,(const xmlChar *)"interval"));
      test_set->confidence.level     = set_confidence_level(desired_level);
      test_set->confidence.interval  = set_confidence_interval(dsrd_interval);
    }

    gen_report = get_report_function(my_cmd);
  
    /* top of confidence interval loop */
    while (sample_count < max_count) {
      /* execute commands and check for events in a loop */
      cmd = commands;
      while (cmd != NULL && rc == NPE_SUCCESS) {
        if (debug) {
          fprintf(where,"report_stats_command: calling process_command\n");
          fprintf(where,"report_stats_command: name '%s' tid '%s'\n",
                  cmd->name, xmlGetProp(cmd,(const xmlChar *)"tid"));
          fflush(where);
        }
        rc = process_command(cmd);
        if (rc != NPE_SUCCESS) {
          /* how should we handle command errors ? sgb */
          fprintf(where,"report_stats_command: calling report_cmd_error\n");
          report_cmd_error(rc);
          break;
        }
        /* should events be processed in this internal command loop ?  */
#ifdef OFF
        rc = process_events();
        if (rc != NPE_SUCCESS) {
          /* how should we handle event errors ? sgb */
          report_event_error(rc);
          break;
        }
#endif
        cmd = cmd->next;
      }

      sample_count++;
      test_set->confidence.count = sample_count;

      (gen_report)(test_set, report_flags, output_file);

      if ((test_set->confidence.value >= 0) && (min_count <= sample_count)) {
        break;
      }
    }
  } else {
    rc = NPE_TEST_SET_NOT_FOUND;
  }

  NETPERF_DEBUG_EXIT(debug,where);
  
  return(rc);
}



static int
create_test_set(xmlNodePtr cmd, uint32_t junk)
{
  int          rc   = NPE_SUCCESS;
  int          i,j,l,n1,n2,n3;
  int          first,last;
  xmlChar     *setid;
  char        *tests = NULL;
  tset_t      *test_set;
  test_t      *test;
  tset_elt_t  *new_link;
  tset_elt_t  *link;
  tset_elt_t  *next_link;
  char         tid[32];
  char         s1[8];
  char         s2[8];
  char         s3[8];

  if (debug) {
    fprintf(where,"create_test_set: entering\n");
    fflush(where);
  }

  setid        = xmlGetProp(cmd,(const xmlChar *)"set_name");
  
  test_set     = (tset_t *)malloc(sizeof(tset_t));

  if (test_set != NULL) {
    memset(test_set,0,sizeof(tset_t));
    test_set->id           =  setid;
    test_set->tests_in_set = xmlGetProp(cmd,(const xmlChar *)"tests_in_set");
    tests = (char *)(test_set->tests_in_set);
    test_set->get_confidence = get_confidence;
  } else {
    /* test set could not be allocated  report error */
    rc = NPE_MALLOC_FAILED5;
  }

  if (debug) {
    fprintf(where,"create_test_set: setname = '%s'\n",setid);
    fprintf(where,"create_test_set: tests_in_set ='%s'\n",
            test_set->tests_in_set);
    fprintf(where,"create_test_set: tests   ='%s'\n",tests);
    fflush(where);
  }
  l = strlen(tests);
  i = 0;
  while ((l > 0) && (rc == NPE_SUCCESS)) {
    first = -1;
    last  = -1;
    n1    = -1;
    n2    = -1;
    n3    = -1;
    sscanf(&tests[i],"%1s%d%n:%1s%d%n,",s1,&first,&n1,s2,&last,&n2);
    sscanf(&tests[i],"%s%n,",tid,&n3);
    sscanf(s1,"%[a-zA-Z]",s3);
    if ((n1 > 1) && (first >= 0) && (s1[0] == s3[0])) {
      if (n2 < 0) {
        /* single test */
        last = first;
        i = i + n1;
        l = l - n1;
        if (n3 > n1) {
          i = i + 1;
          l = l - 1;
        }
      } else if ((last < first) || (s1[0] != s2[0])) {
        rc = NPE_BAD_TEST_RANGE;
        continue;
      } else {
        i = i + n2;
        l = l - n2;
        if (n3 > n2) {
          i = i + 1;
          l = l - 1;
        }
      }
    } else {
      rc = NPE_BAD_TEST_ID;
      continue;
    }
    /* test range */
    for (j = first; j <= last; j++) {
      sprintf(&tid[1],"%d",j);
      test = find_test_in_hash((xmlChar *)tid);
      if (test) {
        new_link  =  (tset_elt_t *)malloc(sizeof(tset_elt_t));
        if (new_link) {
          new_link->id    = test->id;
          new_link->test  = test;
          new_link->prev  = test_set->last;
          new_link->next  = NULL;
          if (test_set->last) { 
            test_set->last->next  =  new_link;
          }
          test_set->last  = new_link;
          if (test_set->tests == NULL) {
            test_set->tests  = new_link;
          }
          test_set->num_tests++;
        } else {
          rc = NPE_MALLOC_FAILED6;
          break;
        }
      } else {
        rc = NPE_TEST_NOT_FOUND;
      }
    }
  }
  
  if (rc == NPE_SUCCESS) {
    rc = add_test_set_to_hash(test_set);
  }

  if (rc != NPE_SUCCESS) {
    /* clean up all the allocated memory for this test_set */
    if (test_set) {
      link = test_set->tests;
      while (link) {
        next_link = link->next;
        free(link);
        link      = next_link;
      }
      free(test_set);
    }
  }
  if (debug) {
    fprintf(where,"create_test_set: exiting rc = %d\n",rc);
    fflush(where);
  }
  return(rc);
}


static int
delete_test_set(xmlNodePtr cmd, uint32_t junk)
{
  int          rc        = NPE_SUCCESS;
  tset_t      *test_set;
  xmlChar     *setid;
  tset_elt_t  *link;
  tset_elt_t  *next_link;

  if (debug) {
    fprintf(where,"delete_test_set: entering\n");
    fflush(where);
  }

  setid = xmlGetProp(cmd,(const xmlChar *)"set_name");
  
  test_set  =  find_test_set_in_hash(setid);
  if (test_set) {
    delete_test_set_from_hash(setid);
    link = test_set->tests;
    while (link) {
      next_link = link->next;
      free(link);
      link      = next_link;
    }
    free(test_set);
  }
  
  NETPERF_DEBUG_EXIT(debug,where);

  return(rc);
}


static int
show_test_set(xmlNodePtr cmd, uint32_t junk)
{
  int          rc   = NPE_SUCCESS;
  xmlChar     *setid;
  tset_t      *test_set;

  setid = xmlGetProp(cmd,(const xmlChar *)"set_name");
  test_set  =  find_test_set_in_hash(setid);
  if (test_set) {
    printf("%s = '%s'\n",setid,test_set->tests_in_set);
  }
  return(rc);
}


static int
exec_local_command(xmlNodePtr cmd, uint32_t junk)
{
  int      rc   = NPE_SUCCESS;
  xmlChar *command;

  command = xmlGetProp(cmd,(const xmlChar *)"command");
  fprintf(where,"%s: %s\n", __func__, command);
  fflush(where);
  system((char *)command);
  return(rc);
}


static int
exit_netperf_command(xmlNodePtr cmd, uint32_t junk)
{
  return(NPE_COMMANDED_TO_EXIT_NETPERF);
}


static int
stats_command(xmlNodePtr cmd, uint32_t junk)
{
  int          rc   = NPE_SUCCESS;
  xmlChar     *tid;
  test_t      *test;
  server_t    *server;
  tset_t      *test_set;
  char        *msg;

  switch (junk) {
  case GET_STATS:
    msg="get_stats";
    break;
  case CLEAR_STATS:
    msg="clear_stats";
    break;
  default:
    msg="unknown_stats";
  }

  if (debug) {
    fprintf(where,"stats_command: entered for '%s' command\n",msg);
    fflush(where);
  }
  tid       = xmlGetProp(cmd,(const xmlChar *)"tid");
  test_set = find_test_set_in_hash(tid);
  if (test_set) {
    tset_elt_t *set_elt = test_set->tests;
    while (set_elt) {
      test = set_elt->test;
      if (test) {
        if (debug) {
          fprintf(where,"test_id = '%s'  server_id ='%s'\n",
                  test->id,test->server_id);
          fflush(where);
        }
        server = find_server_in_hash(test->server_id);
        xmlSetProp(cmd,(const xmlChar *)"tid", test->id);
        rc = write_to_control_connection(server->source,
					 cmd,
					 server->id,
					 my_nid);
        if (rc != NPE_SUCCESS) {
          test->state  = TEST_ERROR;
          test->err_rc = rc;
          test->err_fn = (char *)__func__;
        }
      }
      set_elt = set_elt->next;
    }
    xmlSetProp(cmd,(const xmlChar *)"tid", test_set->id);
  } else {
    test = find_test_in_hash(tid);
    if (test) {
      if (debug) {
        fprintf(where,"test_id = '%s'  server_id ='%s'\n",
                test->id,test->server_id);
        fflush(where);
      }
      server = find_server_in_hash(test->server_id);
      xmlSetProp(cmd,(const xmlChar *)"tid", test->id);
      rc = write_to_control_connection(server->source,
				       cmd,
				       server->id,
				       my_nid);
      if (rc != NPE_SUCCESS) {
        test->state  = TEST_ERROR;
        test->err_rc = rc;
        test->err_fn = (char *)__func__;
      }
    }
  }
  
  NETPERF_DEBUG_EXIT(debug,where);

  return(rc);
}
  


static int
wait_command(xmlNodePtr cmd, uint32_t junk)
{
  int seconds;
  xmlChar *string;

  wait_for_tests_to_enter_requested_state(cmd);
  
  /* again with the ass-u-me either the string will be there, or that
     atoi will do what we want it to if given a null pointer... so,
     lets fix that sort of bug - again...  raj 2005-10-27 */
  string = xmlGetProp(cmd,(const xmlChar *)"seconds");
  if (string) {
    seconds = atoi((char *)string);
  }
  else {
    seconds = 0;
  }
  if (seconds) {
    g_usleep(seconds*1000000);
  }
  return(NPE_SUCCESS);
}


static int
comment_command(xmlNodePtr cmd, uint32_t junk)
{
  return(NPE_SUCCESS);
}


static int
close_command(xmlNodePtr cmd, uint32_t junk)
{
  int      rc;
  xmlChar  *sid;
  server_t *server;
  
  sid    = xmlGetProp(cmd,(const xmlChar *)"sid");
  server = find_server_in_hash(sid);
  xmlSetProp(cmd,(const xmlChar *)"sid", server->id);
  rc = write_to_control_connection(server->source,
				   cmd,
				   server->id,
				   my_nid);
  if (rc != NPE_SUCCESS) {
    server->state  = NSRV_ERROR;
    server->err_rc = rc;
    server->err_fn = (char *)__func__;
  }
  while ((server->state != NSRV_ERROR) &&
         (server->state != NSRV_CLOSE) &&
         (server->state != NSRV_EXIT ))  {
    g_usleep(1000000);
  }
  delete_server(sid);
  return(rc);
}


static int
unknown_command(xmlNodePtr cmd, uint32_t junk)
{
  fprintf(where,"process_command: unknown command in file %s\n",cname);
  fflush(where);
  return(NPE_SUCCESS);
}



typedef int (*cmd_func_t)(xmlNodePtr cmd, uint32_t);

struct netperf_cmd {
  char      *cmd_name;
  cmd_func_t cmd_func;
  uint32_t   new_state;
};

struct netperf_cmd netperf_cmds[] = {
  /* Command name,     function,                new_state       */
  { "measure",         request_state_change,    TEST_MEASURE    },
  { "load",            request_state_change,    TEST_LOADED     },
  { "get_stats",       stats_command,           GET_STATS       },
  { "clear_stats",     stats_command,           CLEAR_STATS     },
#ifdef OFF
  { "get_stats",       get_stats_command,       0               },
  { "get_sys_stats",   get_sys_stats_command,   0               },
  { "clear_stats",     clear_stats_command,     0               },
  { "clear_sys_stats", clear_sys_stats_command, 0               },
#endif
  { "report_stats",    report_stats_command,    0               },
  { "idle",            request_state_change,    TEST_IDLE       },
  { "init",            request_state_change,    TEST_INIT       },
  { "die",             request_state_change,    TEST_DEAD       },
  { "wait",            wait_command,            0               },
  { "create_test_set", create_test_set,         0               },
  { "delete_test_set", delete_test_set,         0               },
  { "show_test_set",   show_test_set,           0               },
  { "comment",         comment_command,         0               },
  { "close",           close_command,           0               },
  { "exit",            exit_netperf_command,    0               },
  { "exec_local",      exec_local_command,      0               },
  { "exec_remote",     unknown_command,         0               },
  { NULL,              unknown_command,         0               }
};

static int
process_command(xmlNodePtr cmd)
{
  int rc = NPE_SUCCESS;
  struct netperf_cmd *which_cmd;
  
  if (debug || want_verbose) {
    fprintf(where,"process_command: received %s command\n",cmd->name);
    fflush(where);
  }
  which_cmd = &netperf_cmds[0];
  while (which_cmd->cmd_name != NULL) {
    if (!xmlStrcmp(cmd->name,(xmlChar *)which_cmd->cmd_name)) {
      if (debug) {
        fprintf(where,"process_command: calling %s_command\n",cmd->name);
        fflush(where);
      }
      rc = (which_cmd->cmd_func)(cmd, which_cmd->new_state);
      break;
    }
    which_cmd++;
  }
  return(rc);
}

static int
process_events()
{
  return(NPE_SUCCESS);
}

static int
process_commands_and_events()
{
  int         rc  = NPE_SUCCESS;
  xmlDocPtr   doc;
  xmlNodePtr  commands;
  xmlNodePtr  cmd;

  if (debug) {
    fprintf(where,"process_commands_and_events: calling parse_xml_file\n");
    fflush(where);
  }

  /* default command file commands
        load t_all command
        measure t_all command
        wait for 20 seconds
        load t_all command
        idle t_all command
        get_stats t_all command
        die t_all command
     end of default command file commands */

  /* parse command file */
  commands = parse_xml_file(iname, (const xmlChar *)"commands", &doc);

  /* execute commands and events in a loop */
  if (commands != NULL) {
    cmd = commands->xmlChildrenNode;
  }
  else {
    cmd = NULL;
  }
  while (cmd != NULL && rc == NPE_SUCCESS) {
    if (debug) {
      fprintf(where,"process_commands_and_events: calling process_command\n");
      fflush(where);
    }
    rc = process_command(cmd);
    if (rc != NPE_SUCCESS) {
      /* how should we handle command errors ? sgb */
      fprintf(where,"process_commands_and_events: calling report_cmd_error\n");
      report_cmd_error(rc);
      break;
    }
    if (debug) {
      fprintf(where,"process_commands_and_events: calling process_events\n");
      fflush(where);
    }
    rc = process_events();
    if (rc != NPE_SUCCESS) {
      /* how should we handle event errors ? sgb */
      report_event_error(rc);
      break;
    }
    cmd = cmd->next;
  }
  xmlFreeDoc(doc);

  NETPERF_DEBUG_EXIT(debug,where);

  return(rc);
}

int
main(int argc, char **argv)
{
  int       rc = NPE_SUCCESS;
  GOptionContext *option_context;
  gboolean ret;
  GError *error=NULL;
#ifdef G_OS_WIN32
  WSADATA	wsa_data ;

#endif

  program_name = argv[0];

  g_thread_init(NULL);

  where = stderr;

#ifdef G_OS_WIN32
  /* Initialize the winsock lib ( version 2.2 ) */
  if ( WSAStartup(MAKEWORD(2,2), &wsa_data) == SOCKET_ERROR ){
    g_fprintf(where,"WSAStartup() failed : %d\n", GetLastError()) ;
    return 1 ;
  }
#endif

  option_context = g_option_context_new(" - netperf4 netperf options");
  g_option_context_add_main_entries(option_context,netperf_entries, NULL);
  ret = g_option_context_parse(option_context, &argc, &argv, &error);
  if (error) {
    /* it sure would be nice to know how to trigger the help output */
    g_error("%s g_option_context_parse %s %d %s\nUse netserver --help for help\n",
	      __func__,
	      g_quark_to_string(error->domain),
	      error->code,
	      error->message);
    g_clear_error(&error);
  }

  xmlInitParser();
  xmlKeepBlanksDefault(0);

  xmlDoValidityCheckingDefaultValue = 1;
  xmlLoadExtDtdDefaultValue |= XML_COMPLETE_ATTRS;
  parse_xml_file(cname, (const xmlChar *)"netperf", &config_doc);

  netperf_init();

  instantiate_netservers();

  launch_worker_threads();
 
  rc = wait_for_tests_to_initialize();

  if (rc == NPE_SUCCESS) {
    rc = process_commands_and_events();
  }

  /* add code to cleanup all threads */
  /* exit with state */
  if (debug) {
    fprintf(where,"netperf4 exiting with rc = %d\n",rc);
    fflush(where);
  }
  exit(rc);
}

