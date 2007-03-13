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

*/

#include <glib-object.h>

#include <stdio.h>
#include "netperf-netserver.h"

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

int debug = 0;
FILE *where;

const xmlChar *my_nid = (const xmlChar *)"netperf";

typedef gboolean (*netperf_interactive_cmd_func_t)(gchar *arguments,gpointer data);

static gboolean parse_interactive_new_command(char *arguments, gpointer data);
static gboolean parse_interactive_set_command(char *arguments, gpointer data);
static gboolean parse_interactive_read_command(char *arguments, gpointer data);
static gboolean parse_interactive_show_command(char *arguments, gpointer data);
static gboolean parse_interactive_load_command(char *arguments, gpointer data);
static gboolean parse_interactive_measure_command(char *arguments, gpointer data);
static gboolean parse_interactive_init_command(char *arguments, gpointer data);
static gboolean parse_interactive_idle_command(char *arguments, gpointer data);
static gboolean parse_interactive_stats_command(char *arguments, gpointer data);
static gboolean parse_interactive_die_command(char *arguments, gpointer data);
static gboolean unknown_interactive_command(char *arguments, gpointer data);

struct netperf_interactive_cmds {
  char *command_name;
  netperf_interactive_cmd_func_t command_func;
};

struct netperf_interactive_cmds interactive_command_table[] = {
  /* command_name         command_func */
  { "set",                 parse_interactive_set_command},
  { "show",                parse_interactive_show_command},
  { "new",                 parse_interactive_new_command},
  { "read",                parse_interactive_read_command},
  { "load",                parse_interactive_load_command},
  { "measure",             parse_interactive_measure_command},
  { "init",                parse_interactive_init_command},
  { "idle",                parse_interactive_idle_command},
  { "stats",               parse_interactive_stats_command},
  { "die",                 parse_interactive_die_command},
  { NULL,                  unknown_interactive_command}
};

GHashTable *server_hash;
GHashTable *test_hash;
GHashTable *test_set_hash;

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
int want_gui;                   /* should we try to launch the GUI? */
int want_interactive;           /* does the user want an interactive session */
int want_quiet;                 /* --quiet, --silent */
int want_brief;                 /* --brief */
int want_verbose;               /* --verbose */
int want_batch = 0;             /* set when -i specified */


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
    {"gui", 'g', 0, G_OPTION_ARG_NONE, &want_gui, "Enable the Graphical User Interface", NULL},
    {"interactive", 'I', 0, G_OPTION_ARG_NONE, &want_interactive, "Enable the Graphical User Interface", NULL},
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


void print_netserver_hash_entry(gpointer key, gpointer value, gpointer user_data)
{
  NetperfNetserver *netserver;
  guint state,req_state;
  gchar *id;
  xmlNodePtr node;
  xmlNodePtr tmp_node;
  xmlDocPtr  tmp_doc;
  xmlChar *buf;
  guint buflen;

  g_print("netserver: %s, object %p, user_data %p\n",
	  (char *)key,
	  value,
	  user_data);
  /* the key :) thing to remember is that the pointer to the netserver
     object is the value, not the key */
  netserver = value;
  g_object_get(netserver,
	       "state", &state,
	       "req-state", &req_state,
	       "id", &id,
	       "node", &node,
	       NULL);
  g_print("state: %d req_state: %d id %s\n",state,req_state,id);
  g_print("node: %p\n",node);

  /* seems we cannot just dump a node absent a doc... */
  if ((tmp_doc = xmlNewDoc((xmlChar *)"1.0")) != NULL) {
    if ((tmp_node = xmlDocCopyNode(node,tmp_doc,1)) != NULL) {
      xmlDocSetRootElement(tmp_doc,tmp_node);
      xmlDocDumpFormatMemory(tmp_doc,
			     &buf,
			     &buflen,
			     1);
      g_print("node buflen is %d\n%s",buflen,buf);
    }
    else {
      g_print("could not copy node information\n");
    }
    xmlFreeDoc(tmp_doc);
  }
  else {
    g_print("\t\tcould not allocate doc for node information\n");
  }
}

void print_test_hash_entry(gpointer key, gpointer value, gpointer user_data)
{
  NetperfTest *test;
  guint state,req_state;
  gchar *id;
  xmlNodePtr node;
  xmlNodePtr tmp_node;
  xmlDocPtr  tmp_doc;
  xmlChar *buf;
  guint buflen;

  /* the key :) thing to remember is that the pointer to the test
     object is the value, not the key */
  test = value;
  g_object_get(test,
	       "state", &state,
	       "req-state", &req_state,
	       "id", &id,
	       "node", &node,
	       NULL);
  g_print("test: %s, object %p, user_data %p id %s state %d req_state %d node %p\n",
	  (char *)key,
	  value,
	  user_data,
	  id,
	  state,
	  req_state,
	  node);

  /* seems we cannot just dump a node absent a doc... */
  if ((tmp_doc = xmlNewDoc((xmlChar *)"1.0")) != NULL) {
    if ((tmp_node = xmlDocCopyNode(node,tmp_doc,1)) != NULL) {
      xmlDocSetRootElement(tmp_doc,tmp_node);
      xmlDocDumpFormatMemory(tmp_doc,
			     &buf,
			     &buflen,
			     1);
      g_print("node buflen is %d\n%s",buflen,buf);
    }
    else {
      g_print("could not copy node information\n");
    }
    xmlFreeDoc(tmp_doc);
  }
  else {
    g_print("\t\tcould not allocate doc for node information\n");
  }
}

static void netperf_init() 
{
  /* the key for the server hash is the id string, so we use the
     glib-provided g_str_hash as the hash function and g_str_equal for
     the key equality test function */
  NETPERF_DEBUG_ENTRY(debug,where);
  server_hash = g_hash_table_new(g_str_hash,g_str_equal);

  test_hash = g_hash_table_new(g_str_hash, g_str_equal);

  test_set_hash = g_hash_table_new(g_str_hash, g_str_equal);
  NETPERF_DEBUG_EXIT(debug,where);
}



/* REVISIT - need to add test_set support */
static void
wait_for_test_to_enter_requested_state(gchar *testid, guint state) 
{

  NetperfTest *test;
  guint test_state,test_req_state;

  NETPERF_DEBUG_ENTRY(debug,where);

  test = g_hash_table_lookup(test_set_hash, testid);

  if (test) {
    /* this was a test set */
  }
  else {
    /* wasn't a test_set - is it a plain test? */
    test = g_hash_table_lookup(test_hash,testid);
    
    if (test) {
      do {
	g_object_get(test,
		     "state", &test_state,
		     "req-state",&test_req_state,
		     NULL);
      } while ((test_state != test_req_state) && (test_state != TEST_ERROR));
      if (TEST_ERROR == test_state) {
	/* there is something wrong with the test, that is bad REVISIT */
      }
    }
    else {
      /* we didn't find a test, that is bad REVISIT */
    }
  }
  NETPERF_DEBUG_EXIT(debug,where);

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

static gboolean periodic_callback(gpointer data)
{
  static int i = 1;
  g_print("Hello there again for the %d",i);
  if (i > 3) g_print("th time\n");
  else if (i > 1) g_print("nd time\n");
  else g_print("st time\n");
  i++;

  return TRUE;
}

static gboolean parse_interactive_new_command(char *arguments,
					      gpointer data)
{
  gboolean return_value = TRUE;
  g_print("the arguments to the NEW command were %s\n",
	  arguments);

  return return_value;
}

static gboolean unknown_interactive_command(char *arguments,
					    gpointer data) {
  g_print("The command you entered %s is presently unknown\n",
	  arguments);

  return TRUE;
}

static gboolean parse_interactive_set_command(char *arguments,
					      gpointer data)
{
  gboolean return_value = TRUE;

  g_print("the arguments to the SET command were %s\n",
	  arguments);
  return return_value;
}

static gboolean parse_interactive_show_test_command(char *arguments,
						    gpointer data)
{
  gboolean return_value = TRUE;
  NetperfTest *test;
  gchar **split;

  NETPERF_DEBUG_ENTRY(debug,where);

  split = g_strsplit_set(arguments,
			 " \r\n",
			 2);

  if (g_strcasecmp(split[0],"all") == 0) {
    g_hash_table_foreach(test_hash,print_test_hash_entry,data);
  }
  else {
    test = g_hash_table_lookup(test_hash,split[0]);
    if (NULL != test) {
      print_test_hash_entry(split[0],test,data);
    }
    else {
      g_print("test id %s unknown!\n",
	      split[0]);
    }
  }
  
  g_strfreev(split);

  NETPERF_DEBUG_EXIT(debug,where);
  return return_value;
}

static gboolean parse_interactive_show_netserver_command(char *arguments,
						    gpointer data)
{
  gboolean return_value = TRUE;
  gchar **split;
  NetperfNetserver *server;

  NETPERF_DEBUG_ENTRY(debug,where);

  split = g_strsplit_set(arguments,
			 " \r\n",
			 2);


  if (g_strcasecmp(split[0],"all") == 0) {
    g_hash_table_foreach(server_hash,print_netserver_hash_entry,data);
  }
  else {
    server = g_hash_table_lookup(server_hash,split[0]);
    if (NULL != server) {
      print_netserver_hash_entry(split[0],server,data);
    }
    else {
      g_print("netserver id %s unknown!\n",
	      split[0]);
    }
  }
  
  g_strfreev(split);
  NETPERF_DEBUG_EXIT(debug,where);
  return return_value;
}

static gboolean execute_interactive_show_all_command(gpointer data)
{
  gboolean return_value = TRUE;

  NETPERF_DEBUG_ENTRY(debug,where);

  NETPERF_DEBUG_EXIT(debug,where);
  return return_value;
}


static gboolean parse_interactive_show_command(char *arguments,
					       gpointer data)
{
  gboolean return_value = TRUE;
  gchar **split;

  NETPERF_DEBUG_ENTRY(debug,where);
  g_print("the arguments to the SHOW command were %s\n",
	  arguments);
  split = g_strsplit_set(arguments,
			" \r\n",
			2);

  if (g_strcasecmp("netserver",split[0]) == 0) 
    return_value = parse_interactive_show_netserver_command(split[1],
							    data);
  else if (g_strcasecmp("test",split[0]) == 0)
    return_value = parse_interactive_show_test_command(split[1],
						       data);
  else if (g_strcasecmp("all",split[0]) == 0)
    return_value = execute_interactive_show_all_command(data);

  g_strfreev(split);
  NETPERF_DEBUG_EXIT(debug,where);
  return return_value;
}

static gboolean parse_interactive_read_command(char *arguments,
					       gpointer data)
{
  gboolean return_value = TRUE;
  g_print("the arguments to the READ command were %s\n",
	  arguments);
  return return_value;
}

static gboolean parse_interactive_load_command(char *arguments,
					       gpointer data)
{
  gboolean return_value = TRUE;
  g_print("the arguments to the LOAD command were %s\n",
	  arguments);
  return return_value;
}

static gboolean parse_interactive_measure_command(char *arguments,
					       gpointer data)
{
  gboolean return_value = TRUE;
  g_print("the arguments to the MEASURE command were %s\n",
	  arguments);
  return return_value;
}

static gboolean parse_interactive_init_command(char *arguments,
					       gpointer data)
{
  gboolean return_value = TRUE;
  g_print("the arguments to the INIT command were %s\n",
	  arguments);
  return return_value;
}

static gboolean parse_interactive_idle_command(char *arguments,
					       gpointer data)
{
  gboolean return_value = TRUE;
  g_print("the arguments to the IDLE command were %s\n",
	  arguments);
  return return_value;
}

static gboolean parse_interactive_stats_command(char *arguments,
					       gpointer data)
{
  gboolean return_value = TRUE;
  g_print("the arguments to the stats command were %s\n",
	  arguments);
  return return_value;
}

static gboolean parse_interactive_die_command(char *arguments,
					       gpointer data)
{
  gboolean return_value = TRUE;
  g_print("the arguments to the DIE command were %s\n",
	  arguments);
  return return_value;
}


/* this is _really_ brain dead parsing, but it should be good enough
   to get us going.  I would be _MORE_ than happy to have something a
   bit more sophisticated if warranted. also, the return should
   probably become a bit more sophisticated one of these days to deal
   with fatal vs non-fatal errors */

static gboolean parse_interactive_command_line(gchar *command_line,
					       gpointer data)
{
  struct netperf_interactive_cmds *command_table_entry;

  gchar **split;
  gboolean return_value = TRUE;
  command_table_entry = interactive_command_table;

  split = g_strsplit_set(command_line,
		     " \r\n", /* space, the final delimiter */
		     2);  /* yes, splitting to a magic constant number
			     of tokens, naughty naughty */

  /* a very simplistic parsing of the split. split[0] should be our
     command, the rest of it the arguments to the command which will
     be parsed by the command-specific parser. the g_strcasecmp
     documentation suggests that this is not a good thing to use, but
     it was also difficult to follow what to use to replace it. for
     the time being, we aren't worried about localization though, so I
     _think_ I'm OK - famous last words. perhaps one of these days
     we'll use one of those tables like used for the command message
     parsing... */

  while (command_table_entry->command_name != NULL) {
    if (g_strcasecmp(command_table_entry->command_name,split[0]) == 0){
      return_value = (command_table_entry->command_func)(split[1],data);
      break;
    }
    command_table_entry++;
  }

  if (NULL == command_table_entry->command_name) {
    g_print("command %s with arguments %s is presently unknown\n",
	    split[0],
	    split[1]);
    return_value = TRUE;
  }
  g_strfreev(split);

  return return_value;
}  

static gboolean read_from_terminal(GIOChannel *source,
				   GIOCondition condition,
				   gpointer data) {

  GIOStatus ret;
  gchar *command_line;
  gsize length;
  gsize terminator_pos;
  GError *error = NULL;
  gboolean return_value = TRUE;

  NETPERF_DEBUG_ENTRY(debug,where);

  ret = g_io_channel_read_line(source,
			       &command_line,
			       &length,
			       &terminator_pos,
			       &error);

  switch (ret)     {
  case G_IO_STATUS_NORMAL:
    return_value = parse_interactive_command_line(command_line,data);
    g_free(command_line);
    break;
  case G_IO_STATUS_EOF:
    g_print("the user tried to ask for end of file on the terminal\n");
    return_value = FALSE; /* time to remove the watch */
    break;

  case G_IO_STATUS_ERROR:
    g_print("Knuth only knows what went wrong, we have G_IO_STATUS_ERROR\n");
    break;

  case G_IO_STATUS_AGAIN:
    g_print("It was deceiving us, we have G_IO_STATUS_AGAIN\n");
    break;
  }

  NETPERF_DEBUG_EXIT(debug,where);

  return return_value;
}

static void add_netservers_tests(xmlNodePtr server_node, NetperfNetserver *server) {
  int        rc = NPE_SUCCESS;
  NetperfTest    *new_test;
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
    new_test = g_object_new(TYPE_NETPERF_TEST,
			    "id", testid,
			    "serverid", serverid,
			    "node", this_test,
			    NULL);
    if (new_test != NULL) { /* we have a new NetperfTest object */
      /* REVISIT - extract the relevant functions please
      rc = get_test_function(new_test,(const xmlChar *)"test_name");
      rc = get_test_function(new_test,(const xmlChar *)"test_decode"); */
      g_hash_table_replace(test_hash,
			   testid,
			   new_test);
    }
    this_test = this_test->next;
  }
}

static void add_netservers_from_config(xmlDocPtr config_doc) {

  int                 rc = NPE_SUCCESS;
  xmlNodePtr          netperf;
  xmlNodePtr          this_netserver;
  NetperfNetserver   *new_server;
  xmlChar            *netserverid;

  NETPERF_DEBUG_ENTRY(debug,where);


  /* first, get the netperf element which was checked at parse time.  The
     netperf element has only netserver elements each with its own unique
     nid ID attribute. Duplicates should have been caught by the xml parser.
     Users must correctly setup configuration files. Syntax errors in the
     netperf configuration file should have been caught by the xml parser
     when it did a syntax check against netperf_docs.dtd sgb 2003-10-7 */

  netperf = xmlDocGetRootElement(config_doc);

  if (NULL != netperf) {
    /* now get the first netserver node */
    this_netserver = netperf->xmlChildrenNode;
    
    while (this_netserver != NULL && rc == NPE_SUCCESS) {
      if (xmlStrcmp(this_netserver->name,(const xmlChar *)"netserver")) {
	if (debug) {
	  g_print("instantiate_netservers skipped a non-netserver node\n");
	}
	this_netserver = this_netserver->next;
	continue;
      }
      netserverid = xmlGetProp(this_netserver,(const xmlChar *)"nid");
      /* REVISIT - make sure the node property gets set */
      new_server = g_object_new(TYPE_NETPERF_NETSERVER,
				"id", netserverid,
				"node", this_netserver,
				NULL);
      if (new_server != NULL) {    /* we have a new netserver object,
				      lets add it to the server_hash */
	g_hash_table_replace(server_hash,
			     netserverid,
			     new_server);
	/* REVISIT must set the node property, and decide what to do
	   about the control connection/object. must also see about
	   adding the tests to the test_hash and the like */
      }
      add_netservers_tests(this_netserver,new_server);
      /* see about the next netserver */
      this_netserver = this_netserver->next;
    }
  }
  else {
    g_print("Yo, there was either nothing to the config file or it had no netservers\n");
  }
  NETPERF_DEBUG_EXIT(debug,where);

}
int main(int argc, char **argv) 
{


  GIOChannel *input_channel;
  guint       input_watch_id;

  GOptionContext *option_context;
  gboolean ret;
  GError *error=NULL;
  
  GMainLoop *loop;

  g_type_init();

#ifdef G_OS_WIN32
  WSADATA	wsa_data ;

#endif

  /* should this be a g_strdup() call instead of an assignment of the
     pointer? */
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

  netperf_init();

  /* OK, all the "non-test" initialization is complete.  Now we have
     to decide if we are going to run a batch command file, or start
     trying to take commands from the user in an interactive
     manner. */

  parse_xml_file(cname, (const xmlChar *)"netperf", &config_doc);

  /* add_netservers_from_config will add all the netservers from the
     config file to the server_hash. it will also cause all the tests
     associated with those netservers to be added to the
     test_hash. whether or not it starts to init those will depend on
     whim and whether or not the user asked for an interactive session */ 
  add_netservers_from_config(config_doc);

  loop = g_main_loop_new(NULL, FALSE);

  if (want_interactive) {
    g_print("Yo, the user wants an interactive session\n");
  }
  else {
    g_print("Hey, the user wants a batch session\n");
  }

  /* ass-u-me-ing that fd0 is stdin is probably a bad thing to do */
  input_channel = g_io_channel_unix_new(0);
  input_watch_id = g_io_add_watch(input_channel,
				  G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
				  read_from_terminal,
				  NULL);

  g_timeout_add(10000,
		(GSourceFunc)periodic_callback,
		NULL);

  g_main_loop_run(loop);

#ifdef notdef
  netserver = g_object_new(TYPE_NETPERF_NETSERVER,
			   "req-state", NSRV_CONNECTED,
			   "id", "george",
			   NULL);
  g_print("netserver george is at %p\n",netserver);

  g_hash_table_replace(server_hash,
		       g_strdup("george"),
		       netserver);

  netserver = g_object_new(TYPE_NETPERF_NETSERVER,
			   "req-state", NSRV_CONNECTED,
			   "id", "fred",
			   NULL);
  g_print("netserver fred is at %p\n",netserver);

  g_hash_table_replace(server_hash,
		       g_strdup("fred"),
		       netserver);

  netserver = g_hash_table_lookup(server_hash,"fred");
  g_print("netserver fred is %p from the hash table\n",
	  netserver);

  /* try iterating through all the entries in the hash table */
  g_hash_table_foreach(server_hash, print_netserver_hash_entry, NULL);
#endif

  return 0;

}
