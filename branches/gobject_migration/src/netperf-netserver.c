#include <glib-object.h>
#include <stdio.h>
#include "netperf-netserver.h"

/* perhaps one of these days, these should become properties of a
   NetperfNetserver? */
extern int debug;
extern FILE *where;

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
  NETSERVER_PROP_REQ_STATE
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
			 we can use it to size the array correctly. */
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
  struct msgs *which_msg;

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
    which_msg = np_msg_handler_base;
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
          if (server->sock != -1) {
            CLOSE_SOCKET(server->sock);
            /* should we delete the server from the server_hash ? sgb */
            break;
          }
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
  test_t    *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"%s: tid = %s  prev_state_req = %d ",
              __func__, testid, test->state_req);
      fflush(where);
    }
    test->state_req = TEST_DEAD;
    if (debug) {
      fprintf(where," new_state_req = %d\n",test->state_req);
      fflush(where);
    }
  }
  return(rc);
}


int
dead_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  xmlChar    *testid;
  test_t     *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
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
  test_t    *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
  if (test != NULL) {
    if (debug || loc_debug) {
      fprintf(where,"error_message: prev_state_req = %d ",test->state_req);
      fflush(where);
    }
    test->err_rc  = atoi((char *)xmlGetProp(msg,(const xmlChar *)"err_rc"));
    test->err_fn  = (char *)xmlGetProp(msg,(const xmlChar *)"err_fn");
    test->err_str = (char *)xmlGetProp(msg,(const xmlChar *)"err_str");
    test->err_no  = atoi((char *)xmlGetProp(msg,(const xmlChar *)"err_no"));
    test->state   = TEST_ERROR;
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
  test_t      *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
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
  test_t      *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
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
  test_t      *test;
  xmlNodePtr  stats;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
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
  test_t      *test;
  xmlNodePtr  sys_stats;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
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
  test_t    *test;
  int        hash_value;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");

  hash_value = TEST_HASH_VALUE(testid);

  /* don't forget to add error checking one day */
  if (debug) {
    fprintf(where,"np_idle_message: waiting for mutex\n");
    fflush(where);
  }
  NETPERF_MUTEX_LOCK(test_hash[hash_value].hash_lock);

  test = test_hash[hash_value].test;
  while (test != NULL) {
    if (!xmlStrcmp(test->id,testid)) {
      /* we have a match */
      break;
    }
    test = test->next;
  }

  if (test != NULL) {
    if (debug) {
      fprintf(where,"np_idle_message: prev_state = %d ",test->state);
      fflush(where);
    }
    test->state = TEST_IDLE;
    /* I'd have liked to have abstracted this with the NETPERF_mumble
       macros, but the return values differ. raj 2006-03-02 */
    g_cond_broadcast(test_hash[hash_value].condition);
  }
  if (debug) {
    fprintf(where,"np_idle_message: unlocking mutex\n");
    fflush(where);
  }
  NETPERF_MUTEX_UNLOCK(test_hash[hash_value].hash_lock);
  return(rc);
}

int
idle_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int        rc = NPE_SUCCESS;
  xmlChar   *testid;
  test_t    *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"idle_message: tid = %s  prev_state_req = %d ",
              testid, test->state_req);
      fflush(where);
    }
    test->state_req = TEST_IDLE;
    if (debug) {
      fprintf(where," new_state_req = %d\n",test->state_req);
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
  test_t    *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"idled_message: tid = %s  prev_state_req = %d ",
              testid, test->state_req);
      fflush(where);
    }
    test->state = TEST_IDLE;
    if (debug) {
      fprintf(where," new_state_req = %d\n",test->state_req);
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
  test_t    *test;
  int        hash_value;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");

  dependency_data = msg->xmlChildrenNode;
  while (dependency_data != NULL) {
    if (!xmlStrcmp(dependency_data->name,(const xmlChar *)"dependency_data")) {
      break;
    }
  }
  
  hash_value = TEST_HASH_VALUE(testid);

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(test_hash[hash_value].hash_lock);

  test = test_hash[hash_value].test;
  while (test != NULL) {
    if (!xmlStrcmp(test->id,testid)) {
      /* we have a match */
      break;
    }
    test = test->next;
  }

  if (test != NULL) {
    if (debug) {
      fprintf(where,"initialized_message: prev_state = %d ",test->state);
      fflush(where);
    }
    if (dependency_data != NULL) {
      test->dependent_data = xmlCopyNode(dependency_data,1);
      if (test->dependent_data != NULL) {
        test->state = TEST_INIT;
      } else {
        test->state = TEST_ERROR;
        /* add additional error information later */
      }
    } else {
      test->state = TEST_INIT;
    }

    /* since the different cond broadcast calls return different
       things, we cannot use the nice NETPERF_mumble abstractions. */

    g_cond_broadcast(test_hash[hash_value].condition);


    if (debug) {
      fprintf(where," new_state = %d rc = %d\n",test->state,rc);
      fflush(where);
    }

  }
  NETPERF_MUTEX_UNLOCK(test_hash[hash_value].hash_lock);
  return(rc);
}


int
load_message(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server)
{
  int        rc = NPE_SUCCESS;
  xmlChar   *testid;
  test_t    *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"load_message: tid = %s  prev_state_req = %d ",
              testid, test->state_req);
      fflush(where);
    }
    test->state_req = TEST_LOADED;
    if (debug) {
      fprintf(where," new_state_req = %d\n",test->state_req);
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
  test_t    *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"loaded_message: prev_state = %d ",test->state);
      fflush(where);
    }
    test->state = TEST_LOADED;
    if (debug) {
      fprintf(where," new_state = %d\n",test->state);
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
  test_t    *test;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"measure_message: prev_state_req = %d ",test->state_req);
      fflush(where);
    }
    test->state_req = TEST_MEASURE;
    if (debug) {
      fprintf(where," new_state_req = %d\n",test->state_req);
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
  test_t    *test;

  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
  if (test != NULL) {
    if (debug) {
      fprintf(where,"measuring_message: prev_state = %d ",test->state);
      fflush(where);
    }
    test->state = TEST_MEASURE;
    if (debug) {
      fprintf(where," new_state = %d\n",test->state);
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
  test_t    *new_test;
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
    new_test = (test_t *)malloc(sizeof(test_t));
    if (new_test != NULL) { /* we have a new test_t structure */
      memset(new_test,0,sizeof(test_t));
      new_test->node      = test_node;
      new_test->id        = testid;
      new_test->server_id = server->id;
      new_test->state     = TEST_PREINIT;
      new_test->new_state = TEST_PREINIT;
      new_test->state_req = TEST_IDLE;
      new_test->debug     = debug;
      new_test->where     = where;
      rc = get_test_function(new_test,(const xmlChar *)"test_name");
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
      rc = add_test_to_hash(new_test);
    }
    if (rc == NPE_SUCCESS) {
      if (debug) {
        fprintf(where,
                "test_message: launching test thread using func %p\n",
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

	rc = launch_thread(&new_test->thread_id,launch_pad,launch_state);
	if (debug) {
	  fprintf(where,
		  "test_message: launched test thread\n");
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
       thread id has been set in the test_t */
    if (rc == NPE_SUCCESS) {
      while (new_test->new_state == TEST_PREINIT) {
        if (debug) {
          fprintf(where,
                  "test_message: waiting on thread\n");
          fflush(where);
        }
        g_usleep(1000000);
      }  /* end wait */
      if (debug) {
        fprintf(where,
                "test_message: test initialization finished on thread\n");
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
        rc2 = set_test_locality(new_test, (char *)loc_type, (char *)loc_value);
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
      new_test->state  = TEST_ERROR;
      new_test->err_rc = rc;
      new_test->err_fn = (char *)__func__;
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
  test_t      *test;
  xmlNodePtr  stats = NULL;

  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
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
        fprintf(where,"sys_stats_message: stats to list %p\n",
                test->received_stats->xmlChildrenNode);
        fflush(where);
      }
    }
    if (stats == NULL) {
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
  test_t      *test;
  xmlNodePtr  stats = NULL;


  testid = xmlGetProp(msg,(const xmlChar *)"tid");
  test   = find_test_in_hash(testid);
  if (test != NULL) {
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

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}
