#include <glib-object.h>
#include <stdio.h>
#include "netperf-netserver.h"

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
