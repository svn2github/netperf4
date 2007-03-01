#ifndef NETPERF_TEST_H
#define NETPERF_TEST_H
/* declarations to provide us with a Test object to be used by
   netperf4's GObject conversion */

typedef enum netperf_test_state {
  TEST_PREINIT,
  TEST_INIT,
  TEST_IDLE,
  TEST_LOADED,
  TEST_MEASURE,
  TEST_ERROR,
  TEST_DEAD
} test_state_t;

typedef void      *(*TestFunc)(void *test_data);
typedef void      *(*TestDecode)(void *statistics);
typedef int        (*TestClear)(void *test_info);
typedef void      *(*TestStats)(void *test_data);

/* first the instance structure. this will look remarkably like the
   old test_t structure. */

typedef struct _NetperfTest {
  /* the parent object - in this case a GObject object */
  GObject parent_instance;

  /* the rest of the test stuff goes here.  how much should be
     "public" and how much "private" I've no idea... */

  gchar   *id;         /* the identifier for this test instance. it
			  will be duplicated with the key used in the
			  GHash to find this object. */

  gchar   *server_id;     /* perhaps this should become a
			     [strong|weak] object reference  to the
			     netserver object? */

  gchar  *node;      /* the XML document node containing the servers
			configuration data */

  guint32  debug;    /* should the test generate debug output? */

  FILE   *where;     /* where should that output go? */

  test_state_t  state;     /* the state netperf or netserver believes
			      the test instance to be in at the
			      moment.  only changed by netperf or
			      netserver  */

  test_state_t  new_state; /* the state the test is currently in.
			      this field is modified by the test when
			      it has transitioned to a new state. */

  test_state_t  state_req; /* the state to which the test has been
			      requested to transition. this field is
			      monitored by the the test thread and
			      when the field is changed the test takes
			      action and changes its state. */

  gint          err_rc;    /* error code received which caused this
			      server to enter the TEST_ERROR state */

  gchar        *err_fn;    /* name of the routine which placed this
			     server into the TEST_ERROR state*/

  char        *err_str;     /* character string which reports the
			       error causing entry to the TEST_ERROR
			       state. */

  int        err_no;        /* The errno returned by the failing
			       syscall */
  
#ifdef notdef
  NETPERF_THREAD_T thread_id;   /* as the different threads packages
				   have differnt concepts of what the
				   opaque thread id should be we will
				   have to do some interesting
				   backflips to deal with them */
#endif
  void   *native_thread_id_ptr; /* a pointer to the "native" thread id
				   of the test thread, used for things
				   like processor affinity since
				   GThread doesn't do that sort of
				   thing :( */

  gchar   *test_name;        /* the ASCII name of the test being
				performed by this test instance. */

  void      *library_handle;   /* the handle passed back by dlopen
                                  when the library containing the
                                  test-specific routines was
                                  opened. */

  TestFunc   test_func;        /* the function pointer returned by
                                  dlsym for the test_name
                                  function. This function is launched
                                  as a thread to initialize the
                                  test. */
  
  TestClear  test_clear;       /* the function pointer returned by
                                  dlsym for the test_clear
                                  function. the function clears all
                                  statistics counters. */
  
  TestStats  test_stats;       /* the function pointer returned by
                                  dlsym for the test_stats
                                  function. this function reads all
                                  statistics counters for the test and
                                  returns an xml statistics node for
                                  the test. */
  
  TestDecode test_decode;      /* the function pointer returned by
                                  dlsym for the test_decode
                                  function. this function is called by
                                  netperf to decode, accumulate, and
                                  report statistics nodes returned by
                                  tests from this library. */

  void  *received_stats;   /* a node to which all test_stats received
			      by netperf from this test are appended
			      as children */

  void  *dependent_data;   /* a pointer to test-specific things to
			      return to the test which depends on this
			      test */

  void  *dependency_data;  /* a pointer to test-specific things
			      returned by the test on which this test
			      is dependent */

  void *test_specific_data;    /* a pointer to test-specific things -
                                  config settings for the test, places
                                  to store results that sort of
                                  stuff... */

  /* we probably need/want some sort of list of test objects which
     depend upon us so we will be able to signal them that their
     dependecies have been met */

} NetperfTest;

/* second, the class structure, which is where we will put any method
   functions we wish to have and any signals we wish to define */

typedef struct _NetperfTestClass {
  /* the parent class, matching what we do with the instance */
  GObjectClass parent_class;

  /* signals */
  void (*new_message)(NetperfTest *test, gpointer message);
  void (*control_closed)(NetperfTest *test);
  void (*dependency_met)(NetperfTest *test);

  /* methods */

} NetperfTestClass;


GType netperf_test_get_type (void);

#define TYPE_NETPERF_TEST (netperf_test_get_type())

#define NETPERF_TEST(object) \
 (G_TYPE_CHECK_INSTANCE_CAST((object), TYPE_NETPERF_TEST, NetperfTest))

#define NETPERF_TEST_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_NETPERF_TEST, MediaClass))

#define IS_NETPERF_TEST(object) \
 (G_TYPE_CHECK_INSTANCE_TYPE((object), TYPE_NETPERF_TEST))

#define IS_NETPERF_TEST_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_NETPERF_TEST))

#define NETPERF_TEST_GET_CLASS(object) \
 (G_TYPE_INSTANCE_GET_CLASS((object), TYPE_NETPERF_TEST, NetperfTestClass))

#endif