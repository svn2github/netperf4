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

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif 

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <glib-object.h>
#include <glib.h>
#include <gmodule.h>

#include "netperf.h"
#include "netperf-test.h"

enum {
  TEST_PROP_DUMMY_0,    /* GObject does not like a zero value for a property id */
  TEST_PROP_ID,
  TEST_PROP_SERVERID,
  TEST_PROP_STATE,
  TEST_PROP_NODE,
  TEST_PROP_REQ_STATE,
  TEST_PROP_TEST_FUNC,
  TEST_PROP_TEST_CLEAR,
  TEST_PROP_TEST_STATS,
  TEST_PROP_TEST_DECODE,
  TEST_PROP_TEST_ADD_DEPENDANT,
  TEST_PROP_TEST_DEL_DEPENDANT
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
			 dependant has finished initializing */
  LAUNCH_THREAD,      /* ask the object to launch a thread to run the
			 actual test code */
  LAST_SIGNAL         /* this should always be the last in the list so
			 we can use it to size the array correctly. */
};

static void netperf_test_new_message(NetperfTest *test, gpointer message);
static void netperf_test_control_closed(NetperfTest *test);
static void netperf_test_dependency_met(NetperfTest *test);
static void netperf_test_launch_thread(NetperfTest *test);

/* a place to stash the id's returned by g_signal_new should we ever
   need to refer to them by their ID. */ 

static guint netperf_test_signals[LAST_SIGNAL] = {0,0,0,0};

char *
netperf_test_state_to_string(int value)
{
  char *state; 

  switch (value) {
  case NP_TST_PREINIT:
    state = "PREINIT";
    break;
  case NP_TST_INIT:
    state = "INIT";
    break;
  case NP_TST_IDLE:
    state = "IDLE";
    break;
  case NP_TST_MEASURE:
    state = "MEASURE";
    break;
  case NP_TST_LOADED:
    state = "LOAD";
    break;
  case NP_TST_ERROR:
    state = "ERROR";
    break;
  case NP_TST_DEAD:
    state = "DEAD";
    break;
  default:
    state = "UNKNOWN";
    break;
  }
  return state;
}

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
  GParamSpec *node_param;
  GParamSpec *serverid_param;
  GParamSpec *id_param;
  GParamSpec *test_func_param;
  GParamSpec *test_clear_param;
  GParamSpec *test_stats_param;
  GParamSpec *test_decode_param;
  GParamSpec *test_add_dependant_param;
  GParamSpec *test_del_dependant_param;

  /* and on with the show */
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS(klass);

  /* we might create GParamSpec descriptions for properties here */
  req_state_param = 
    g_param_spec_uint("req-state", /* identifier */
		      "requested state", /* longer name */
		      "desired state of the test", /* description */
		      NP_TST_PREINIT, /* min value */
		      NP_TST_DEAD,    /* max value */
		      NP_TST_PREINIT, /* def value */
		      G_PARAM_READWRITE); 

  state_param = 
    g_param_spec_uint("state", /* identifier */
		      "test state", /* longer name */
		      "currentstate of the test",
		      NP_TST_PREINIT, /* min value */
		      NP_TST_DEAD,    /* max value */
		      NP_TST_PREINIT, /* def value */
		      G_PARAM_READABLE); /* should this be read only? */

  id_param = 
    g_param_spec_string("id",         /* identifier */
			"identifier", /* longer name */
			"unique test identifier", /* description */
			"unnamed",    /* default value */
			G_PARAM_READWRITE);

  serverid_param =
    g_param_spec_string("serverid",
			"server identifier",
			"the id of the server to which this test belongs",
			"unnamed",
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

  node_param = 
    g_param_spec_pointer("node",
			 "test node",
			 "the xml node containing test information",
			 G_PARAM_READWRITE);

  test_decode_param =
    g_param_spec_pointer("test_decode",
			 "test decode stats",
			 "the function which decode, accumulate and report stats",
			 G_PARAM_READWRITE);

  test_add_dependant_param =
    g_param_spec_pointer("add_dependant",
			 "add dependant test",
			 "add this test object to the list of dependant tests",
			 G_PARAM_READWRITE); /* should this be just
						WRITABLE? */

  /* if we go with a weak pointer reference then we probably don't
     need/want a delete property, but if we simply keep a pointer to
     the test instance then we do */
  test_del_dependant_param =
    g_param_spec_pointer("del_dependant",
			 "delete dependant test",
			 "delete this test object from the list of dependant tests",
			 G_PARAM_READWRITE); /* should this be just
						WRITABLE? */

  
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
				  TEST_PROP_SERVERID,
				  serverid_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_TEST_FUNC,
				  test_func_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_NODE,
				  node_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_TEST_CLEAR,
				  test_clear_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_TEST_STATS,
				  test_stats_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_TEST_DECODE,
				  test_decode_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_TEST_ADD_DEPENDANT,
				  test_add_dependant_param);

  g_object_class_install_property(g_object_class,
				  TEST_PROP_TEST_DEL_DEPENDANT,
				  test_del_dependant_param);

  /* we would set the signal handlers for the class here. we might
     have a signal say for the arrival of a complete message or
     perhaps the cotnrol connection going down or somesuch. */

  klass->new_message = netperf_test_new_message;
  klass->control_closed = netperf_test_control_closed;
  klass->dependency_met = netperf_test_dependency_met;
  klass->launch_thread = netperf_test_launch_thread;

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

  netperf_test_signals[LAUNCH_THREAD] =
    g_signal_new("launch_thread",
		 TYPE_NETPERF_TEST,
		 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		 G_STRUCT_OFFSET(NetperfTestClass, launch_thread),
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

/* we are sent this signal when the test on which we are dependant has
   finished its initialization, which means we can get our dependency
   data */ 
static void netperf_test_dependency_met(NetperfTest *test)
{
  g_print("The dependency for test %p named %s has been met\n",
	  test,
	  test->id);

  return;
}


/* this routine exists because there is no architected way to get a
   "native" thread id out of a GThread.  we need a native thread ID to
   allow the main netserver thread to bind a test thread to a
   specified CPU/processor set/locality domain. so, we use the
   launch_pad routine to allow the newly created thread to store a
   "pthread_self" into the test structure. the netserver thread, when
   it see's the test thread is in the idle state can then assign the
   affinity to the thread. raj 2006-03-30 */

void *
netperf_test_launch_pad(void *arg) {

  NetperfTest *test = arg;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

#ifdef G_THREADS_IMPL_POSIX
  /* hmm, I wonder if I have to worry about alignment */
#ifdef _AIX
  /* bless its heart, AIX wants its CPU binding routine to be given a
     kernel thread ID and not a pthread id.  isn't that special. */
  test->native_thread_id_ptr = malloc(sizeof(tid_t));
  if (test->native_thread_id_ptr) {
    *(tid_t *)(test->native_thread_id_ptr) = thread_self();
  }
  if (debug) {
    fprintf(where,"%s my thread id is %d\n",__func__,thread_self());
    fflush(where);
  }
#elif defined(__sun)
#include <sys/lwp.h>
  /* well, bless _its_ heart, Solaris wants something other than a
     pthread_id for its binding calls too.  isn't that special... raj
     2006-06-28 */
  test->native_thread_id_ptr = malloc(sizeof(lwpid_t));
  if (test->native_thread_id_ptr) {
    *(lwpid_t *)(test->native_thread_id_ptr) = _lwp_self();
  }
  if (test->debug) {
    fprintf(where,"%s my thread id is %d\n",__func__,_lwp_self());
    fflush(where);
  }
#else
#include <pthread.h>
  test->native_thread_id_ptr = malloc(sizeof(pthread_t));
  if (test->native_thread_id_ptr) {
    *(pthread_t *)(test->native_thread_id_ptr) = pthread_self();
  }
  if (test->debug) {
    fprintf(test->where,"%s my thread id is %ld\n",__func__,pthread_self());
    fflush(test->where);
  }
#endif
#else
#ifdef __sun
#include <sys/lwp.h>
  /* well, bless _its_ heart, Solaris wants something other than a
     pthread_id for its binding calls too.  isn't that special...  No,
     your eyes are not deceiving you - this code is appearing in two
     places because it would seem that glib-2.0 on Solaris may be
     compiled using Sun's old threads stuff rather than pthreads. raj
     2006-06-28 */
  test->native_thread_id_ptr = malloc(sizeof(lwpid_t));
  if (test->native_thread_id_ptr) {
    *(lwpid_t *)(test->native_thread_id_ptr) = _lwp_self();
  }
  if (test->debug) {
    fprintf(test->where,"%s my thread id is %d\n",__func__,_lwp_self());
    fflush(test->where);
  }
#else
  test->native_thread_id_ptr = NULL;
#endif
#endif
  /* and now, call the routine we really want to run. at some point we
     should bring those values onto the stack so we can free the
     thread_launch_state_t I suppose. */
  return (test->test_func)(test);
}

static void netperf_test_launch_thread(NetperfTest *test)
{

  g_thread_create_full(netperf_test_launch_pad,   /* what to run */
		       test, /* what it should use */
		       0,    /* default stack size */
		       FALSE, /* not joinable */
		       TRUE,  /* bound - make it system scope */
		       G_THREAD_PRIORITY_NORMAL,
		       NULL);

  /* REVISIT we really should be checking return values here and
     possibly setting corresponding values in the test structure on
     failure etc. */
}

static int get_test_function(NetperfTest *test, const xmlChar *func)
{

  GModule *lib_handle = test->library_handle;
  gboolean ret;
  xmlChar *fname;
  void    *fptr = NULL;
  int      fnlen = 0;
  int      rc = NPE_FUNC_NAME_TOO_LONG;
  char     func_name[NETPERF_MAX_TEST_FUNCTION_NAME];
  xmlChar *la_file;

  /* should this be global debug, or test->debug? */
  if (test->debug) {
    g_fprintf(test->where,
	      "%s enter test %p func %s\n",
	      __func__,
	      test,
	      func);
    fflush(test->where);
  }

  if (lib_handle == NULL) {
    /* load the library for the test */
    la_file   = xmlGetProp(test->node,(const xmlChar *)"library");
    if (test->debug) {
      g_fprintf(test->where,
		"%s looking to open library %p\n",
		__func__,
		la_file);
      fflush(test->where);
    }

    lib_handle = open_library(la_file);
    free(la_file);
    test->library_handle = lib_handle;
  }

  fname = xmlGetProp(test->node,func);
  if (!fname) {
    fnlen = 0;
  }
  else {
    fnlen = strlen((char *)fname);
  }
  if (test->debug) {
    if (fname) {
      g_fprintf(test->where,"func = '%s'  fname = '%s' fname[0] = %c fnlen = %d\n",
	      (char *)func, fname, fname[0], fnlen);
    }
    else {
      g_fprintf(test->where,"func = '%s'  fname = '' fname[0] = '' fnlen = %d\n",
	      (char *)func, fnlen);
    }
    fflush(test->where);
  }

  if (fnlen < NETPERF_MAX_TEST_FUNCTION_NAME) {
    if (fnlen) {
      strcpy(func_name,(char *)fname);
      rc = NPE_SUCCESS;
    } else {
      xmlChar *tname      = test->test_name;
      int      tnlen = strlen((char *)tname);

      strcpy(func_name,(char *)tname);
      if (test->debug) {
        g_fprintf(test->where,"func_name = '%s' tnlen = %d\n",
                (char *)func_name, tnlen);
        fflush(test->where);
      }

      if (!xmlStrcmp(func,(const xmlChar *)"test_clear")) {
        if (strlen("_clear_stats") < (size_t)(NETPERF_MAX_TEST_FUNCTION_NAME-tnlen)) {
          strcat(func_name,"_clear_stats");
          rc = NPE_SUCCESS;
        } else {
          rc = NPE_FUNC_NAME_TOO_LONG;
        }
      } else if (!xmlStrcmp(func,(const xmlChar *)"test_stats")) {
        if (strlen("_get_stats") < (size_t)(NETPERF_MAX_TEST_FUNCTION_NAME-tnlen)) {
          strcat(func_name,"_get_stats");
          rc = NPE_SUCCESS;
        } else {
          rc = NPE_FUNC_NAME_TOO_LONG;
        }
      } else if (!xmlStrcmp(func,(const xmlChar *)"test_decode")) {
        if (strlen("_decode_stats") < (size_t)(NETPERF_MAX_TEST_FUNCTION_NAME-tnlen)) {
          strcat(func_name,"_decode_stats");
          rc = NPE_SUCCESS;
        } else {
          rc = NPE_FUNC_NAME_TOO_LONG;
        }
      } else {
        rc = NPE_UNKNOWN_FUNCTION_TYPE;
      }
    }
    if (test->debug) {
      g_fprintf(test->where,"func_name = '%s'\n", (char *)func_name);
      fflush(test->where);
    }
    if (rc == NPE_SUCCESS) {
      ret = g_module_symbol(lib_handle,func_name,&fptr);
      if (test->debug) {
        g_fprintf(test->where,"symbol lookup of func_name '%s' returned %p\n",
                func_name, fptr);
        if (fptr == NULL) {
	  g_fprintf(test->where,"g_module_symbol error '%s'\n",g_module_error());
        }
        fflush(test->where);
      }
      if (fptr == NULL) {
        if (lib_handle) {
          rc = NPE_FUNC_NOT_FOUND;
        } else {
          rc = NPE_LIBRARY_NOT_LOADED;
        }
      }
    }
  } else {
    rc = NPE_FUNC_NAME_TOO_LONG;
  }

  if (!xmlStrcmp(func,(const xmlChar *)"test_name")) {
    test->test_name    = fname;
    test->test_func    = (NetperfTestFunc)fptr;
  } else if (!xmlStrcmp(func,(const xmlChar *)"test_clear")) {
    test->test_clear   = (NetperfTestClear)fptr;
    xmlFree(fname);
  } else if (!xmlStrcmp(func,(const xmlChar *)"test_stats")) {
    test->test_stats   = (NetperfTestStats)fptr;
    xmlFree(fname);
  } else if (!xmlStrcmp(func,(const xmlChar *)"test_decode")) {
    test->test_decode  = (NetperfTestDecode)fptr;
    xmlFree(fname);
  } else {
    rc = NPE_UNKNOWN_FUNCTION_TYPE;
    xmlFree(fname);
  }

  if (rc != NPE_SUCCESS) {
    if (test->debug) {
      g_fprintf(test->where,
              "get_test_function: error %d occured getting function %s\n",
              rc,func_name);
      fflush(test->where);
    }
  }

  return(rc);
}

/* get and set property routines */
static void netperf_test_set_property(GObject *object,
					   guint prop_id,
					   const GValue *value,
					   GParamSpec *pspec) {
  NetperfTest *test;
  NetperfTest *dependent;
  
  GList *item;
  guint req_state;
  xmlChar *test_func;

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

  case TEST_PROP_SERVERID:
    test->server_id = g_value_dup_string(value);
    break;

  case TEST_PROP_TEST_FUNC:
    test->test_func = g_value_get_pointer(value);
    break;

  case TEST_PROP_NODE:
    test->node = g_value_get_pointer(value);
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

  case TEST_PROP_TEST_ADD_DEPENDANT:
    dependent = g_value_get_pointer(value);
    item = g_list_append(NULL, dependent);
    g_object_add_weak_pointer(dependent, &item->data);
    test->dependent_tests = g_list_concat(item, test->dependent_tests);
    break;

  case TEST_PROP_TEST_DEL_DEPENDANT:
    dependent = g_value_get_pointer(value);
    g_print("Yo, add the code to delete a dependant!\n");
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

  case TEST_PROP_SERVERID:
    g_value_set_string(value, test->server_id);
    break;

  case TEST_PROP_NODE:
    g_value_set_pointer(value, test->node);
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

    /* it doesn't seem to make much sense to have "get" for add/del a
       dependant test so we will let those fall-through to the default
       case */

  case TEST_PROP_TEST_ADD_DEPENDANT:
  case TEST_PROP_TEST_DEL_DEPENDANT:
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}
