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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#include "netperf.h"
#include "netlib.h"
#include "netperf-netserver.h"
#include "netperf-test.h"

/* perhaps one of these days, these should become properties of a
   NetperfNetserver? */
extern int debug;
extern FILE *where;
extern GHashTable *test_hash;

static int clear_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int clear_sys_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int close_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int closed_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int configured_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int dead_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int die_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int error_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int get_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int get_sys_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int idle_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int idled_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int initialized_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int interval_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int load_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int loaded_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int measure_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int measuring_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int netserver_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int np_idle_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int np_version_check(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int ns_version_check(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int snap_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int test_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int sys_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int test_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int unexpected_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);
static int unknown_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);

const struct netperf_msgs   NP_Msgs[] = {
  /* Message name, function,             StateBitMap */
#ifdef OFF
  { "cleared",         cleared_message,          0x00000000 },
  { "configured",      configured_message,       0x00000000 },
#endif
  { "measuring",       measuring_message,        0x00000010 },
  { "loaded",          loaded_message,           0x00000010 },
  { "idled",           np_idle_message,          0x00000018 },
  { "test_stats",      test_stats_message,       0x00000010 },
  { "sys_stats",       sys_stats_message,        0x00000010 },
  { "initialized",     initialized_message,      0x00000018 },
  { "error",           error_message,            0x00000018 },
  { "dead",            dead_message,             0x00000018 },
  { "closed",          closed_message,           0x00000018 },
  { "version",         np_version_check,         0x00000004 },
  { NULL,              unknown_message,          0xFFFFFFFF }
};

const struct netperf_msgs   NS_Msgs[] = {
  /* Message name,     function,                StateBitMap */
#ifdef OFF
  { "clear",           clear_message,            0x00000000 },
  { "error",           error_message,            0x00000000 },
  { "interval",        interval_message,         0x00000000 },
  { "snap",            snap_message,             0x00000000 },
#endif
  { "measure",         measure_message,          0x00000010 },
  { "load",            load_message,             0x00000010 },
  { "idle",            idle_message,             0x00000010 },
  { "get_stats",       get_stats_message,        0x00000010 },
  { "get_sys_stats",   get_sys_stats_message,    0x00000010 },
  { "clear_stats",     clear_stats_message,      0x00000010 },
  { "clear_sys_stats", clear_sys_stats_message,  0x00000010 },
  { "initialized",     initialized_message,      0x00000010 },
  { "test",            test_message,             0x00000014 },
  { "die",             die_message,              0x00000030 },
  { "close",           close_message,            0x00000030 },
  { "version",         ns_version_check,         0x00000001 },
  { NULL,              unknown_message,          0xFFFFFFFF }
};

enum {
  NETSERVER_PROP_DUMMY_0,    /* GObject does not like a zero value for a property id */
  NETSERVER_PROP_ID,
  NETSERVER_PROP_STATE,
  NETSERVER_PROP_REQ_STATE,
  NETSERVER_PROP_CONTROL_CONNECTION
};

/* some forward declarations to make the compiler happy regardless of
   the order in which things appear in the file */
static void netperf_netserver_set_property(GObject *object,
					   guint prop_id,
					   const GValue *value,
					   GParamSpec *pspec);

static void netperf_netserver_get_property(GObject *object,
					   guint prop_id,
					   GValue *value,
					   GParamSpec *pspec);

static void netperf_netserver_class_init(NetperfNetserverClass *klass);

/* our netserver signals */
enum {
  NEW_MESSAGE,        /* this would be the control connection object
			 telling us a complete message has arrived. */
  CONTROL_CLOSED,     /* this would be the control connection object
			 telling us the controll connection died. */
  LAST_SIGNAL         /* this should always be the last in the list so
			 we can use it to size the array correctly. of
			 course, we may never actually use the array,
			 but the tutorial code from which we are
			 working happened to do so... */
};

static void netperf_netserver_new_message(NetperfNetserver *netserver, gpointer message);
static void netperf_netserver_control_closed(NetperfNetserver *netserver);

/* a place to stash the id's returned by g_signal_new should we ever
   need to refer to them by their ID. */ 

static guint netperf_netserver_signals[LAST_SIGNAL] = {0,0};

GType netperf_netserver_get_type(void) {
  static GType netperf_netserver_type = 0;

  if (!netperf_netserver_type) {
    static const GTypeInfo netperf_netserver_info = {
      sizeof(NetperfNetserverClass),   /* class structure size */
      NULL,                            /* base class initializer */
      NULL,                            /* base class finalizer */
      (GClassInitFunc)netperf_netserver_class_init, /* class initializer */
      NULL,                            /* class finalizer */
      NULL,                            /* class data */
      sizeof(NetperfNetserver),        /* instance structure size */
      1,                               /* number to preallocate */
      NULL,                            /* instance initializer */
      NULL                             /* function table */
    };

    netperf_netserver_type = 
      g_type_register_static(G_TYPE_OBJECT,      /* parent type */
			     "NetperfNetserver", /* type name */
			     &netperf_netserver_info, /* the information */
			     0);                 /* flags */
  }

  return netperf_netserver_type;
}

static void netperf_netserver_class_init(NetperfNetserverClass *klass)
{
  /* GParamSpecs for properties go here */
  GParamSpec *state_param;
  GParamSpec *req_state_param;
  GParamSpec *id_param;
  GParamSpec *control_connection_param;

  /* and on with the show */
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS(klass);

  /* we might create GParamSpec descriptions for properties here */
  state_param = 
    g_param_spec_uint("state", /* identifier */
		      "netserver state", /* longer name */
		      "currentstate of the netserver",
		      NETSERVER_PREINIT, /* min value */
		      NETSERVER_EXIT,    /* max value */
		      NETSERVER_PREINIT, /* dev value */
		      G_PARAM_READABLE); /* should this be read only? */
  req_state_param = 
    g_param_spec_uint("req-state", /* identifier */
		      "requested state", /* longer name */
		      "desired state of the netserver", /* description
							   */
		      NETSERVER_PREINIT, /* min value */
		      NETSERVER_EXIT,    /* max value */
		      NETSERVER_PREINIT, /* def value */
		      G_PARAM_READWRITE); 

  id_param = 
    g_param_spec_string("id",         /* identifier */
			"identifier", /* longer name */
			"unique netserver identifier", /* description */
			"unnamed",    /* default value */
			G_PARAM_READWRITE);

  control_connection_param =
    g_param_spec_pointer("control_connection", /* used to setup a weak
						  pointer to the
						  control connection
						  object */
			 "control connection",
			 "the control connection this netserver should use",
			 G_PARAM_READWRITE);

  /* overwrite the base object methods with our own get and set
     property routines */

  g_object_class->set_property = netperf_netserver_set_property;
  g_object_class->get_property = netperf_netserver_get_property;

  /* we would install our properties here */
  g_object_class_install_property(g_object_class,
				  NETSERVER_PROP_STATE,
				  state_param);

  g_object_class_install_property(g_object_class,
				  NETSERVER_PROP_REQ_STATE,
				  req_state_param);

  g_object_class_install_property(g_object_class,
				  NETSERVER_PROP_ID,
				  id_param);

  g_object_class_install_property(g_object_class,
				  NETSERVER_PROP_CONTROL_CONNECTION,
				  control_connection_param);

  /* we would set the signal handlers for the class here. we might
     have a signal say for the arrival of a complete message or
     perhaps the cotnrol connection going down or somesuch. */

  klass->new_message = netperf_netserver_new_message;
  klass->control_closed = netperf_netserver_control_closed;

  netperf_netserver_signals[NEW_MESSAGE] = 
    g_signal_new("new_message",            /* signal name */
		 TYPE_NETPERF_NETSERVER,   /* class type id */
		 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, /* options,
							   not sure if
							   G_SIGNAL_DETAILED
							   is
							   indicated */
		 G_STRUCT_OFFSET(NetperfNetserverClass, new_message),
		 NULL,                     /* no accumulator */
		 NULL,                     /* so no accumulator data */
		 g_cclosure_marshal_VOID__POINTER, /* return void,
						      take pointer */
		 G_TYPE_NONE,              /* type of return value */
		 1,                        /* one additional parameter */
		 G_TYPE_POINTER);          /* and its type */
  netperf_netserver_signals[CONTROL_CLOSED] = 
    g_signal_new("control_closed",
		 TYPE_NETPERF_NETSERVER,
		 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		 G_STRUCT_OFFSET(NetperfNetserverClass, control_closed),
		 NULL,
		 NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE,
		 0);
}

/* a bunch of the message processing code, originally from netmsg.c */
int
process_message(NetperfNetserver *server, xmlDocPtr doc)
{
  int loc_debug = 0;
  int rc = NPE_SUCCESS;
  int cur_state = 0;
  xmlChar *fromnid;
  xmlNodePtr msg;
  xmlNodePtr cur;
  struct netperf_msgs *which_msg;

  NETPERF_DEBUG_ENTRY(debug,where);

  msg = xmlDocGetRootElement(doc);
  if (msg == NULL) {
    fprintf(stderr,"empty document\n");
    fflush(stderr);
    xmlFreeDoc(doc);
    /* somehow I don't think that returning NPE_SUCCESS is the right
       thing here? raj 2005-10-26 */
    return(rc);
  }
  fromnid = xmlGetProp(msg,(const xmlChar *)"fromnid");

  if (server != NULL)  cur_state = 1 << server->state;
    
  if (debug) {
    fprintf(where,"process_message: received '%s' message from server %s\n",
            msg->xmlChildrenNode->name, fromnid);
    fprintf(where,"process_message: servers current state is %d\n", cur_state);
    fflush(where);
  }
  for (cur = msg->xmlChildrenNode; cur != NULL; cur = cur->next) {
    which_msg = server->message_state_table;
    while (which_msg->msg_name != NULL) {
      if (xmlStrcmp(cur->name,(xmlChar *)which_msg->msg_name)) {
        which_msg++;
        continue;
      }
      if (which_msg->valid_states & cur_state) {
        rc = (which_msg->msg_func)(cur,doc,server);
        if (rc != NPE_SUCCESS) {
          fprintf(where,"process_message: received %d from %s\n",
                  rc, which_msg->msg_name);
          fflush(where);
          server->state = NSRV_ERROR;
	  /* REVISIT this is where we would likely tell the control
	     connection object that it should go away */
	  /* some sort of object signal to the control connection, or
	     perhaps a property set or a method invokation off of
	     server->control_connection - after making sure it is
	     non-NULL of course... */
            /* should we delete the server from the server_hash ? sgb */
	  break;
        }
      } else {
        if (debug || loc_debug) {
          fprintf(where,
                  "process_message:state is %d got unexpected '%s' message.\n",
                  cur_state,
                  cur->name);
          fflush(where);
        }
      }
      which_msg++;
    }
  }
  xmlFreeDoc(doc);
  if (fromnid) free(fromnid);

  NETPERF_DEBUG_EXIT(debug,where);

  return(rc);
}


/*
   netperf verify the version message from a netserver
   Valid responses from netperf are a configuration message if versions
   match or to close the connection if the version does not match.
*/

int
np_version_check(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int rc;
  if (!xmlStrcmp(xmlGetProp(msg,(const xmlChar *)"vers"),NETPERF_VERSION) &&
      !xmlStrcmp(xmlGetProp(msg,(const xmlChar *)"updt"),NETPERF_UPDATE)  &&
      !xmlStrcmp(xmlGetProp(msg,(const xmlChar *)"fix"), NETPERF_FIX))     {
    /* versions match */
    if (debug) {
      fprintf(where,"np_version_check: version check successful\n");
      fflush(where);
    } 
    rc = NPE_SUCCESS;
  } else {
    /* versions don't match */
    if (debug) {
      fprintf(where,"np_version_check: version check failed\n");
      fflush(where);
    }
    rc = NPE_BAD_VERSION;
  }
  return(rc);
}


/*
   generate a version message and send it out the control
   socket.  XML may be the wizziest thing since sliced bits, but it
   sure does lead to some rather cumbersome coding... raj 2003-02-27
*/
int
send_version_message(NetperfNetserver *server, const xmlChar *fromnid)
{
  int rc = NPE_SUCCESS;
  xmlNodePtr message;


  if ((message = xmlNewNode(NULL,(xmlChar *)"version")) != NULL) {
    /* set the properties of the version message -
       the version, update, and fix levels  sgb 2003-02-27 */
    if ((xmlSetProp(message,(xmlChar *)"vers",NETPERF_VERSION) != NULL) &&
        (xmlSetProp(message,(xmlChar *)"updt",NETPERF_UPDATE)  != NULL) &&
        (xmlSetProp(message,(xmlChar *)"fix", NETPERF_FIX)     != NULL))  {
      /* still smiling */
      /* almost there... */
      rc = write_to_control_connection(server->source,
				       message,
				       server->id,
				       fromnid);
      if (rc != NPE_SUCCESS) {
        if (debug) {
          fprintf(where,
                  "send_version_message: write_to_control_connection failed\n");
          fflush(where);
        }
      }
    } else {
      if (debug) {
        fprintf(where,
                "send_version_message: an xmlSetProp failed\n");
        fflush(where);
      }
      rc = NPE_SEND_VERSION_XMLSETPROP_FAILED;
    }
    /* now that we are done with it, best to free the message node */
    xmlFreeNode(message);
  }
  else {
    if (debug) {
      fprintf(where,
              "send_version_message: xmlNewNode failed\n");
      fflush(where);
    }
    rc = NPE_SEND_VERSION_XMLNEWNODE_FAILED;
  }
  if (debug) {
    if (rc != NPE_SUCCESS) {
      fprintf(where,"send_version_message: error status %d\n",rc);
      fflush(where);
    }
  }
  return(rc);
}


/*
   netserver verify the version message from a netperf
   Valid responses from a netserver in an error message if version
   does not match or a version check response if it matches.
*/

int
ns_version_check(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *netperf)
{
  int rc = NPE_SUCCESS;

  if (!xmlStrcmp(xmlGetProp(msg,(const xmlChar *)"vers"),NETPERF_VERSION) &&
      !xmlStrcmp(xmlGetProp(msg,(const xmlChar *)"updt"),NETPERF_UPDATE)  &&
      !xmlStrcmp(xmlGetProp(msg,(const xmlChar *)"fix"), NETPERF_FIX))     {
    /* versions match */
    netperf->state      =  NSRV_VERS;
    netperf->state_req  =  NSRV_WORK;
    rc = send_version_message(netperf,netperf->my_nid);
  } else {
    /* versions don't match */
    if (debug) {
      fprintf(where,"ns_version_chk: version check failed\n");
      fflush(where);
    }
    rc = NPE_BAD_VERSION;
  }
  return(rc);
}


int
die_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int        rc = NPE_SUCCESS;
  xmlChar   *testid;
  NetperfTest   *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  /* since this is not really the test object file, it really
     shouldn't be accessing some of these things directly? */
  if (test != NULL) {
    if (debug) {
      gint state,state_req = -1;
      g_object_get(test,
		   "state", &state,
		   "req-state", &state_req,
		   NULL);
      fprintf(where,"%s: tid = %s  prev_state_req = %d ",
              __func__, testid, state_req);
      fflush(where);
    }
    test->state_req = TEST_DEAD;
    g_object_set(test,
		 "req-state", TEST_DEAD,
		 NULL);
    if (debug) {
      fprintf(where,
	      "%s requested tid %s to enter TEST_DEAD\n",
	      __func__,
	      testid);
      fflush(where);
    }
  }
  return(rc);
}


int
dead_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  xmlChar    *testid;
  NetperfTest    *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"%s: tid = %s  prev_state_req = %d ",
              __func__, testid, test->state_req);
      fflush(where);
    }
    test->state = TEST_DEAD;
    if (debug) {
      fprintf(where," new_state_req = %d\n",test->state_req);
      fflush(where);
    }
  }
  return(NPE_SUCCESS);
}


int
error_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int loc_debug = 0;
  int        rc = NPE_SUCCESS;
  xmlChar   *testid;
  NetperfTest   *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug || loc_debug) {
      fprintf(where,"error_message: prev_state_req = %d ",test->state_req);
      fflush(where);
    }
    test->err_rc  = atoi((char *)xmlGetProp(msg,(const xmlChar *)"err_rc"));
    test->err_fn  = (char *)xmlGetProp(msg,(const xmlChar *)"err_fn");
    test->err_str = (char *)xmlGetProp(msg,(const xmlChar *)"err_str");
    test->err_no  = atoi((char *)xmlGetProp(msg,(const xmlChar *)"err_no"));
    /* test->state   = TEST_ERROR; the stuff above should probably be
       using a property set too...*/
    g_object_set(test,
		 "req-state", TEST_ERROR,
		 NULL);

    if (debug || loc_debug) {
      fprintf(where," new_state_req = %d\n",test->state_req);
      fprintf(where,"\terr in test function %s  rc = %d\n",
              test->err_fn,test->err_rc);
      fprintf(where,"\terror message '%s' errno = %d\n",
              test->err_str,test->err_no);
      fflush(where);
    }
  }
  return(rc);
}


int
clear_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int          rc = NPE_SUCCESS;
  xmlChar     *testid;
  NetperfTest     *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,
            "clear_stats_message: test_state = %d calling %p\n",
            test->state,
            test->test_clear);
      fflush(where);
    }
    rc = (test->test_clear)(test);
    if (rc != NPE_SUCCESS) {
      if (debug) {
        fprintf(where,
                "clear_stats_message: clear of statistics failed\n");
        fflush(where);
      }
    }
  }
  return(rc);
}


int
clear_sys_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int          rc = NPE_SUCCESS;
  xmlChar     *testid;
  NetperfTest     *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,
            "clear_sys_stats_message: test %p test_state = %d calling %p\n",
            test,
            test->state,
            test->test_clear);
      fflush(where);
    }
    rc = (test->test_clear)(test);
    if (rc != NPE_SUCCESS) {
      if (debug) {
        fprintf(where,
                "clear_sys_stats_message: clear of statistics failed\n");
        fflush(where);
      }
    }
  }
  return(rc);
}


int
get_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int          rc = NPE_SUCCESS;
  xmlChar     *testid;
  NetperfTest     *test;
  xmlNodePtr  stats;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"get_stats_message: test_state = %d\n",test->state);
      fflush(where);
    }
    stats = (test->test_stats)(test);
    rc = write_to_control_connection(server->source,
				     stats,
				     server->id,
				     server->my_nid);
    if (rc != NPE_SUCCESS) {
      if (debug) {
        fprintf(where,
                "%s: write_to_control_connection failed\n",
                __func__);
        fflush(where);
      }
    }
  }
  return(rc);
}


int
get_sys_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int          rc = NPE_SUCCESS;
  xmlChar     *testid;
  NetperfTest     *test;
  xmlNodePtr  sys_stats;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"%s: test_state = %d ",
              __func__, test->state);
      fflush(where);
    }
    sys_stats = (test->test_stats)(test);
    rc = write_to_control_connection(server->source,
				     sys_stats,
				     server->id,
				     server->my_nid);
    if (rc != NPE_SUCCESS) {
      if (debug) {
        fprintf(where,
                "%s: write_to_control_connection failed\n",
                 __func__);
        fflush(where);
      }
    }
  }
  return(rc);
}


int
np_idle_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int        rc = NPE_SUCCESS;
  xmlChar   *testid;
  NetperfTest   *test;

  testid = xmlGetProp(msg,(const xmlChar *)"tid");

  test = g_hash_table_lookup(test_hash,(gchar *) testid);

  /* REVISIT - this should use netperftest object property calls */
  if (test != NULL) {
    gint prev_state = -1;
    g_object_get(test,
		 "state", &prev_state,
		 NULL);
    if (debug) {
      fprintf(where,"%s: prev_state = %d ",__func__,test->state);
      fflush(where);
    }
    /* when we set the state to TEST_IDLE, we expect that the property
       setting code will walk a list of dependent tests to inform them
       of the state change of this test. */
    g_object_set(test,
		 "state", TEST_IDLE,
		 NULL);
  }

  return(rc);
}

int
idle_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int        rc = NPE_SUCCESS;
  xmlChar   *testid;
  NetperfTest   *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"idle_message: tid = %s  prev_state_req = %d ",
              testid, test->state_req);
      fflush(where);
    }
    g_object_set(test,
		 "req-state", TEST_IDLE,
		 NULL);

    if (debug) {
      fprintf(where,
	      "%s has set the state of test %p to TEST_IDLE\n",
	      __func__,
	      test);
      fflush(where);
    }
  }
  return(rc);
}

int
idled_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int        rc = NPE_SUCCESS;
  xmlChar   *testid;
  NetperfTest   *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug) {
      gint state_req = -1;
      g_object_get(test,
		   "req-state", &state_req,
		   NULL);
      fprintf(where,
	      "%s: tid = %s  prev_state_req = %d ",
	      __func__,
              testid,
	      state_req);
      fflush(where);
    }
    g_object_set(test,
		 "state", TEST_IDLE,
		 NULL);
    test->state = TEST_IDLE;
    if (debug) {
      fprintf(where,
	      "%s requested test %p enter TEST_IDLE\n",
	      __func__,
	      test);
      fflush(where);
    }
  }
  return(rc);
}

int
close_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  xmlChar    *flag;


  /* what should we really do with this flag??  sgb 2005-12-5 */
  flag = xmlGetProp(msg,(const xmlChar *)"flag");
  if (debug) {
    fprintf(where,"%s: flag = '%s'  state = %d req_state = %d\n",
            __func__, flag, server->state, server->state_req);
    fflush(where);
  }
  if (!xmlStrcmp(flag,(const xmlChar *)"LOOP")) {
    server->state_req = NSRV_CLOSE;
  }
  else {
    server->state_req = NSRV_EXIT;
  }
  return(NPE_SUCCESS);
}

int
closed_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  xmlChar   *flag;


  /* what should we really do with this flag??  sgb 2005-12-5 */
  flag = xmlGetProp(msg,(const xmlChar *)"flag");
  if (debug) {
    fprintf(where,"%s: flag = '%s'  state = %d req_state = %d\n",
            __func__, flag, server->state, server->state_req);
    fflush(where);
  }
  if (!xmlStrcmp(flag,(const xmlChar *)"LOOPING")) {
    server->state = NSRV_CLOSE;
  }
  else {
    server->state = NSRV_EXIT;
  }
  return(NPE_SUCCESS);
}

int
initialized_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  
  int        rc = NPE_SUCCESS;
  xmlNodePtr dependency_data;
  xmlChar   *testid;
  NetperfTest   *test;
  int        hash_value;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");

  dependency_data = msg->xmlChildrenNode;
  while (dependency_data != NULL) {
    if (!xmlStrcmp(dependency_data->name,(const xmlChar *)"dependency_data")) {
      break;
    }
  }
  
  test = g_hash_table_lookup(test_hash, (gchar *)testid);

  if (test != NULL) {
    if (debug) {
      fprintf(where,"initialized_message: prev_state = %d ",test->state);
      fflush(where);
    }
    if (dependency_data != NULL) {
      test->dependent_data = xmlCopyNode(dependency_data,1);
      if (test->dependent_data != NULL) {
	g_object_set(test,
		     "state", TEST_INIT,
		     NULL);
      } else {
	g_object_set(test,
		     "state", TEST_ERROR,
		     NULL);
        /* add additional error information later */
      }
    } else {
      g_object_set(test,
		   "state", TEST_INIT,
		   NULL);
    }

    /* this is where there would be a netperftest property setting
       call - above to set the state, and so no more condition
       variable stuff */

    if (debug) {
      gint state = -1;
      g_object_get(test,
		   "state",&state,
		   NULL);
      fprintf(where,"%s test %p new_state = %d rc = %d\n",
	      __func__,
	      test,
	      state,
	      rc);
      fflush(where);
    }

  }

  return(rc);
}


int
load_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int        rc = NPE_SUCCESS;
  xmlChar   *testid;
  NetperfTest   *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"load_message: tid = %s  prev_state_req = %d ",
              testid, test->state_req);
      fflush(where);
    }
    g_object_set(test,
		 "req-state",TEST_LOADED,
		 NULL);
    if (debug) {
      fprintf(where,"%s requested test %p enter state TEST_LOADED\n",
	      __func__,
	      test);
      fflush(where);
    }
  }
  return(rc);
}

int
loaded_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int        rc = NPE_SUCCESS;
  xmlChar   *testid;
  NetperfTest   *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"loaded_message: prev_state = %d ",test->state);
      fflush(where);
    }
    g_object_set(test,
		 "state", TEST_LOADED,
		 NULL);
    if (debug) {
      fprintf(where,"%s requested test %p enter TEST_LOADED\n",
	      __func__,
	      test);
      fflush(where);
    }
  }
  return(rc);
}


int
measure_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int        rc = NPE_SUCCESS;
  xmlChar   *testid;
  NetperfTest   *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"measure_message: prev_state_req = %d ",test->state_req);
      fflush(where);
    }
    g_object_set(test,
		 "req-state", TEST_MEASURE,
		 NULL);
    if (debug) {
      fprintf(where,"%s requested test %p enter TEST_MEASURE\n",
	      __func__,
	      test);
      fflush(where);
    }
  }
  return(rc);
}

int
measuring_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int        rc = NPE_SUCCESS;
  xmlChar   *testid;
  NetperfTest   *test;

  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"measuring_message: prev_state = %d ",test->state);
      fflush(where);
    }
    g_object_set(test,
		 "state", TEST_MEASURE,
		 NULL);
    if (debug) {
      fprintf(where,"%s test %p entered TEST_MEASURE state\n",
	      __func__,
	      test);
      fflush(where);
    }
  }
  return(rc);
}


int
test_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int        rc = NPE_SUCCESS;
  int        rc2;
  NetperfTest   *new_test;
  xmlNodePtr test_node;
  xmlChar   *testid;
  xmlChar   *loc_type = NULL;
  xmlChar   *loc_value = NULL;
  thread_launch_state_t *launch_state;

  NETPERF_DEBUG_ENTRY(debug,where);

  if (server->state != server->state_req) {
    /* set netserver state to NSRV_WORK because receiving a test message
       shows that netperf accepted our version message */
    server->state = server->state_req;
  }
  test_node = msg;
  while (test_node != NULL && rc == NPE_SUCCESS) {
    if (xmlStrcmp(test_node->name,(const xmlChar *)"test")) {
      if (debug) {
        fprintf(where,"test_message: skipped a non-test node\n");
        fflush(where);
      }
      test_node = test_node->next;
      continue;
    }
    testid = xmlGetProp(test_node,(const xmlChar *)"tid");
    new_test = g_object_new(TYPE_NETPERF_TEST,
			    "node", test_node,
			    "id", testid,
			    "server_id", server->id,
			    "state", NP_TST_PREINIT,
			    "new_state", NP_TST_PREINIT,
			    "req-state", NP_TST_IDLE,
			    "debug", debug,
			    "where", where,
			    NULL);
    if (new_test != NULL) { /* we have a new test_t object */
      /* REVISIT we need to set the function pointers too */
      if (rc == NPE_SUCCESS) {
        rc = get_test_function(new_test,(const xmlChar *)"test_clear");
      }
      if (rc == NPE_SUCCESS) {
        rc = get_test_function(new_test,(const xmlChar *)"test_stats");
      }
      if (rc == NPE_SUCCESS) {
        rc = get_test_function(new_test,(const xmlChar *)"test_decode");
      }

    } else {
      rc = NPE_MALLOC_FAILED2;
    }
    if (rc == NPE_SUCCESS) {
      g_hash_table_replace(test_hash,
			   testid,
			   new_test);
    }
    if (rc == NPE_SUCCESS) {
      if (debug) {
        fprintf(where,
                "%s: launching test thread using func %p\n",
		__func__,
                new_test->test_func);
        fflush(where);
      }
      launch_state =
	(thread_launch_state_t *)malloc(sizeof(thread_launch_state_t));
      if (launch_state) {
	launch_state->data_arg = new_test;
	launch_state->start_routine = new_test->test_func;

	/* before we launch the thread, we should bind ourselves to
	   the cpu to which the thread will be bound (if at all) to
	   make sure that stuff like stack allocation happens on the
	   desired CPU. raj 2006-04-11 */

	loc_type  = xmlGetProp(test_node,(const xmlChar *)"locality_type");
	loc_value = xmlGetProp(test_node,(const xmlChar *)"locality_value");
	if ((loc_type != NULL) && (loc_value != NULL)) {
	  /* we use rc2 because we aren't going to fail the run if the
	     affinity didn't work, but eventually we should emit some
	     sort of warning */
	  rc2 = set_own_locality((char *)loc_type,
				 (char *)loc_value,
				 debug,
				 where);
	}

	/* signal the test object it is time to launch the test thread */
	g_signal_emit_by_name(new_test,
			      "launch_thread");
	if (debug) {
	  fprintf(where,
		  "%s: requested launch of the  test thread for test %p\n",
		  __func__,
		  new_test);
	  fflush(where);
	}
	/* having launched the test thread, we really aught to unbind
	   ourselves from that CPU. at the moment, we do not do that */
      }
      else {
	rc = NPE_MALLOC_FAILED2;
      }
    }

    /* wait for test to initialize, then we will know that it's native
       thread id has been set in the NetperfTest. the sleep is
       something of a kludge we should probably revisit the
       notification mechanism at some point. perhaps we will use a
       signal from the test object back to the netserver object. */
    if (rc == NPE_SUCCESS) {
      while (new_test->new_state == TEST_PREINIT) {
        if (debug) {
          fprintf(where,
                  "%s: waiting on thread\n",
		  __func__);
          fflush(where);
        }
        g_usleep(1000000);
      }  /* end wait */
      if (debug) {
        fprintf(where,
                "%s: test initialization finished on thread\n",
		__func__);
        fflush(where);
      }
    }

    /* now we can set test thread locality */
    if (rc == NPE_SUCCESS) {
      /* we will have already extracted loc_type and loc_value if they
	 exist and there is no possibility of them having spontaneously
	 appeared in the last N lines of code, so we don't need the
	 xmlGetProp calls here :)  raj 2006-04-11 */
      if ((loc_type != NULL) && (loc_value != NULL)) {
	/* we use rc2 because we aren't going to fail the run if the
	   affinity didn't work, but eventually we should emit some
	   sort of warning */
	/* what sort of mechanism should we use here?  we want to have
	   the test thread bound to a specific CPU, and the
	   netlib_mumble.c set_thread_locality routine will want to
	   know about the test's debug and where settings. should we
	   set loc_type and loc_value as properties, should we pass
	   them as a signal, or should we invoke a method? too many
	   choices. for now we will use g_object_set to set the
	   loc_type and loc_value, and once both those are set the
	   test object code will make the call to
	   set_thread_locality. however, the logic might actually be
	   cleaner if we were to use a signal - we could pass both of
	   them at the same time rather than the one at a time setting
	   of a property. */
	g_object_set(new_test,
		     "loc_type", loc_type,
		     "loc_value", loc_value,
		     NULL);
	/* and now we as the launching thread, want to go back to
	   where we were before. */
	rc2 = clear_own_locality((char *)loc_type,debug,where);
      }
      /* however, at this point we need to be sure to free loc_type
	 and/or loc_value. nulling may be a bit over the top, but
	 might as well, this isn't supposed to be that performance
	 critical */
      if (NULL != loc_type)  {
	free(loc_type);
	loc_type = NULL;
      }
      if (NULL != loc_value) {
	free(loc_value);
	loc_value = NULL;
      }
    }

    if (rc != NPE_SUCCESS) {
      /* REVISIT - what culls this test later? */
      g_object_set(new_test,
		   "state", NP_TST_ERROR,
		   "err_rc", rc,
		   "err_fn", (char *)__func__,
		   NULL);
      rc = NPE_TEST_INIT_FAILED;
      break;
    }
    test_node = test_node->next;
  }

  NETPERF_DEBUG_EXIT(debug,where);

  return(rc);
}


int
sys_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int          loc_debug = 0;
  int          rc = NPE_SUCCESS;
  xmlChar     *testid;
  NetperfTest     *test;
  xmlNodePtr  stats = NULL;

  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (debug || loc_debug) {
    fprintf(where,"sys_stats_message: test = %p\n",test);
    fflush(where);
  }
  if (test != NULL) {
    if (test->received_stats == NULL) {
      test->received_stats = xmlNewNode(NULL,(xmlChar *)"received_stats");
      if (debug || loc_debug) {
        fprintf(where,"sys_stats_message: creating received_stats node %p\n",
                test->received_stats);
        fflush(where);
      }
    }
    if (test->received_stats) {
      stats = xmlCopyNode(msg,1);
      xmlAddChild(test->received_stats, stats);
      if (debug || loc_debug) {
        fprintf(where,"%s: stats to list %p\n",
		__func__,
                test->received_stats->xmlChildrenNode);
        fflush(where);
      }
    }
    if (stats == NULL) {
      /* REVISIT - this needs some g_object_set() magic */
      test->state  = TEST_ERROR;
      test->err_rc = NPE_SYS_STATS_DROPPED;
      test->err_fn = (char *)__func__;
    }
  }
  return(rc);
}

int
test_stats_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int          rc = NPE_SUCCESS;
  xmlChar     *testid;
  NetperfTest     *test;
  xmlNodePtr  stats = NULL;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = g_hash_table_lookup(test_hash,(gchar *)testid);
  if (test != NULL) {
    /* REVISIT this needs some g_object_get() magic */
    if (debug) {
      fprintf(where,"test_stats_message: test_state = %d\n",test->state);
      fflush(where);
    }
    if (test->received_stats == NULL) {
      test->received_stats = xmlNewNode(NULL,(xmlChar *)"received_stats");
    }
    if (test->received_stats) {
      stats = xmlCopyNode(msg,1);
      xmlAddChild(test->received_stats, stats);
    }
    if (stats == NULL) {
      /* REVISIT - this needs some g_object_set() magic */
      test->state  = TEST_ERROR;
      test->err_rc = NPE_TEST_STATS_DROPPED;
      test->err_fn = (char *)__func__;
    }
  }
  return(rc);
}


int
unknown_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int rc = NPE_SUCCESS;


  fprintf(where,"Server %s sent a %s message which is unknown by netperf4\n",
          server->id, msg->name);
  fflush(where);
  return(rc);
}

/* signal handler for new message */
static void netperf_netserver_new_message(NetperfNetserver *netserver, gpointer message)
{
  g_print("A new message %p arrived for netserver %p named %s\n",
	  message,
	  netserver,
	  netserver->id);

  process_message(netserver, message);

  return;
}

/* signal handler for control connection closed */
static void netperf_netserver_control_closed(NetperfNetserver *netserver)
{
  g_print("The control connection for netserver %p named %s has been closed\n",
	  netserver,
	  netserver->id);
  return;
}

/* get and set property routines */
static void netperf_netserver_set_property(GObject *object,
					   guint prop_id,
					   const GValue *value,
					   GParamSpec *pspec) {
  NetperfNetserver *netserver;
  guint req_state;
  void *temp_ptr;

  netserver = NETPERF_NETSERVER(object);

  switch(prop_id) {
  case NETSERVER_PROP_STATE:
    g_print("naughty naughty, trying to set the state. rick should make that readonly \n");
    break;
  case NETSERVER_PROP_REQ_STATE:
    req_state = g_value_get_uint(value);
    /* we really need to sanity check the reqeusted state against the
       current state here... */
    netserver->state_req = req_state;
    break;

  case NETSERVER_PROP_ID:
    netserver->id = g_value_dup_string(value);
    break;

  case NETSERVER_PROP_CONTROL_CONNECTION:
    temp_ptr = g_value_get_pointer(value);
    g_object_add_weak_pointer(temp_ptr,&(netserver->control_connection));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void netperf_netserver_get_property(GObject *object,
					   guint prop_id,
					   GValue *value,
					   GParamSpec *pspec) {

  NetperfNetserver *netserver;
  guint state;

  netserver = NETPERF_NETSERVER(object);

  switch(prop_id) {
  case NETSERVER_PROP_STATE:
    g_value_set_uint(value, netserver->state);
    break;

  case NETSERVER_PROP_REQ_STATE:
    g_value_set_uint(value, netserver->state_req);
    break;

  case NETSERVER_PROP_ID:
    g_value_set_string(value, netserver->id);
    break;

  case NETSERVER_PROP_CONTROL_CONNECTION:
    g_value_set_pointer(value, netserver->control_connection);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}
