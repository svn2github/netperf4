#include <glib-object.h>
#include <stdio.h>
#include "netperf-test.h"

enum {
  TEST_PROP_DUMMY_0,    /* GObject does not like a zero value for a property id */
  TEST_PROP_ID,
  TEST_PROP_STATE,
  TEST_PROP_REQ_STATE,
  TEST_PROP_TEST_FUNC,
  TEST_PROP_TEST_CLEAR,
  TEST_PROP_TEST_STATS,
  TEST_PROP_TEST_DECODE
};

/* some forward declarations to make the compiler happy regardless of
   the order in which things appear in the file */
static void netperf_test_set_property(GObject *object,
					   guint prop_id,
					   const GValue *value,
					   GParamSpec *pspec);

static void netperf_test_get_property(GObject *object,
					   guint prop_id,
					   GValue *value,
					   GParamSpec *pspec);

static void netperf_test_class_init(NetperfTestClass *klass);

/* our test signals */
enum {
  NEW_MESSAGE,        /* this would be the control connection object
			 telling us a complete message has arrived. */
  CONTROL_CLOSED,     /* this would be the control connection object
			 telling us the controll connection died. */
  DEPENDENCY_MET,     /* we receive this when the test on which we are
			 dependent has finished initializing */
  LAST_SIGNAL         /* this should always be the last in the list so
			 we can use it to size the array correctly. */
};

static void netperf_test_new_message(NetperfTest *test, gpointer message);
static void netperf_test_control_closed(NetperfTest *test);
static void netperf_test_dependcy_met(NetperfTest *test);

/* a place to stash the id's returned by g_signal_new should we ever
   need to refer to them by their ID. */ 

static guint netperf_test_signals[LAST_SIGNAL] = {0,0};

GType netperf_test_get_type(void) {
  static GType netperf_test_type = 0;

  if (!netperf_test_type) {
    static const GTypeInfo netperf_test_info = {
      sizeof(NetperfTestClass),   /* class structure size */
      NULL,                            /* base class initializer */
      NULL,                            /* base class finalizer */
      (GClassInitFunc)netperf_test_class_init, /* class initializer */
      NULL,                            /* class finalizer */
      NULL,                            /* class data */
      sizeof(NetperfTest),        /* instance structure size */
      1,                               /* number to preallocate */
      NULL,                            /* instance initializer */
      NULL                             /* function table */
    };

    netperf_test_type = 
      g_type_register_static(G_TYPE_OBJECT,      /* parent type */
			     "NetperfTest", /* type name */
			     &netperf_test_info, /* the information */
			     0);                 /* flags */
  }

  return netperf_test_type;
}

static void netperf_test_class_init(NetperfTestClass *klass)
{
  /* GParamSpecs for properties go here */
  GParamSpec *state_param;
  GParamSpec *req_state_param;
  GParamSpec *id_param;
  GParamSpec *test_func_param;
  GParamSpec *test_clear_param;
  GParamSpec *test_stats_param;
  GParamSpec *test_decode_param;

  /* and on with the show */
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS(klass);

  /* we might create GParamSpec descriptions for properties here */
  state_param = 
    g_param_spec_uint("state", /* identifier */
		      "test state", /* longer name */
		      "currentstate of the test",
		      TEST_PREINIT, /* min value */
		      TEST_DEAD,    /* max value */
		      TEST_PREINIT, /* dev value */
		      G_PARAM_READABLE); /* should this be read only? */
  req_state_param = 
    g_param_spec_uint("req-state", /* identifier */
		      "requested state", /* longer name */
		      "desired state of the test", /* description
							   */
		      TEST_PREINIT, /* min value */
		      TEST_DEAD,    /* max value */
		      TEST_PREINIT, /* def value */
		      G_PARAM_READWRITE); 

  id_param = 
    g_param_spec_string("id",         /* identifier */
			"identifier", /* longer name */
			"unique test identifier", /* description */
			"unnamed",    /* default value */
			G_PARAM_READWRITE);

  test_func_param = 
    g_param_spec_pointer("test_func",
			 "test function",
			 "the function launched as a thread to start",
			 G_PARAM_READWRITE);

  test_clear_param =
    g_param_spec_pointer("test_clear",
			 "test clear statistics",
			 "the function which will clear all test stats",
			 G_PARAM_READWRITE);

  test_stats_param =
    g_param_spec_pointer("test_stats",
			 "test statistics",
			 "the function which creates an XML stats node",
			 G_PARAM_READWRITE);

  test_decode_param =
    g_param_spec_pointer("test_decode",
			 "test decode stats",
			 "the function which decode, accumulate and report stats",
			 G_PARAM_READWRITE);

  /* overwrite the base object methods with our own get and set
     property routines */

  g_object_class->set_property = netperf_test_set_property;
  g_object_class->get_property = netperf_test_get_property;

  /* we would install our properties here */
  g_object_class_install_property(g_object_class,
				  TEST_PROP_STATE,
				  state_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_REQ_STATE,
				  req_state_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_ID,
				  id_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_TEST_FUNC,
				  test_func_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_TEST_CLEAR,
				  test_clear_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_TEST_STATS,
				  test_stats_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_TEST_DECODE,
				  test_decode_param);

  /* we would set the signal handlers for the class here. we might
     have a signal say for the arrival of a complete message or
     perhaps the cotnrol connection going down or somesuch. */

  klass->new_message = netperf_test_new_message;
  klass->control_closed = netperf_test_control_closed;

  netperf_test_signals[NEW_MESSAGE] = 
    g_signal_new("new_message",            /* signal name */
		 TYPE_NETPERF_TEST,   /* class type id */
		 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, /* options,
							   not sure if
							   G_SIGNAL_DETAILED
							   is
							   indicated */
		 G_STRUCT_OFFSET(NetperfTestClass, new_message),
		 NULL,                     /* no accumulator */
		 NULL,                     /* so no accumulator data */
		 g_cclosure_marshal_VOID__POINTER, /* return void,
						      take pointer */
		 G_TYPE_NONE,              /* type of return value */
		 1,                        /* one additional parameter */
		 G_TYPE_POINTER);          /* and its type */
  netperf_test_signals[CONTROL_CLOSED] = 
    g_signal_new("control_closed",
		 TYPE_NETPERF_TEST,
		 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		 G_STRUCT_OFFSET(NetperfTestClass, control_closed),
		 NULL,
		 NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE,
		 0);

  netperf_test_signals[DEPENDENCY_MET] =
    g_signal_new("dependecy_met",
		 TYPE_NETPERF_TEST,
		 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		 G_STRUCT_OFFSET(NetperfTestClass, dependency_met),
		 NULL,
		 NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE,
		 0);

}

/* signal handler for new message */
static void netperf_test_new_message(NetperfTest *test, gpointer message)
{
  g_print("A new message %p arrived for test %p named %s\n",
	  message,
	  test,
	  test->id);

  process_message(test, message);

  return;
}

/* signal handler for control connection closed */
static void netperf_test_control_closed(NetperfTest *test)
{
  g_print("The control connection for test %p named %s has been closed\n",
	  test,
	  test->id);
  return;
}

/* we are sent this signal when the test on whichwe are dependent has
   finished its initialization, which means we can get our dependency
   data */ 
static void netperf_test_dependency_met(NetperfTest *test)
{
  g_print("The dependency for test %p named %s has been met\n",
	  test,
	  test->id);

  return;
}

/* get and set property routines */
static void netperf_test_set_property(GObject *object,
					   guint prop_id,
					   const GValue *value,
					   GParamSpec *pspec) {
  NetperfTest *test;
  guint req_state;

  test = NETPERF_TEST(object);

  switch(prop_id) {
  case TEST_PROP_STATE:
    g_print("naughty naughty, trying to set the state. rick should make that readonly \n");
    break;

  case TEST_PROP_REQ_STATE:
    req_state = g_value_get_uint(value);
    /* we really need to sanity check the reqeusted state against the
       current state here... and perhaps do some other work as
       well... */
    test->state_req = req_state;
    break;

  case TEST_PROP_ID:
    test->id = g_value_dup_string(value);
    break;

  case TEST_PROP_TEST_FUNC:
    test->test_func = g_value_get_pointer(value);
    break;

  case TEST_PROP_TEST_CLEAR:
    test->test_clear = g_value_get_pointer(value);
    break;

  case TEST_PROP_TEST_STATS:
    test->test_stats = g_value_get_pointer(value);
    break;

  case TEST_PROP_TEST_DECODE:
    test->test_decode = g_value_get_pointer(value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void netperf_test_get_property(GObject *object,
					   guint prop_id,
					   GValue *value,
					   GParamSpec *pspec) {

  NetperfTest *test;
  guint state;

  test = NETPERF_TEST(object);

  switch(prop_id) {
  case TEST_PROP_STATE:
    g_value_set_uint(value, test->state);
    break;

  case TEST_PROP_REQ_STATE:
    g_value_set_uint(value, test->state_req);
    break;

  case TEST_PROP_ID:
    g_value_set_string(value, test->id);
    break;

  case TEST_PROP_TEST_FUNC:
    g_value_set_pointer(value, test->test_func);
    break;

  case TEST_PROP_TEST_CLEAR:
    g_value_set_pointer(value, test->test_clear);
    break;

  case TEST_PROP_TEST_STATS:
    g_value_set_pointer(value, test->test_stats);
    break;

  case TEST_PROP_TEST_DECODE:
    g_value_set_pointer(value, test->test_decode);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}