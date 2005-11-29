#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <pthread.h>

/* we want to get the definition of uint32_t et al */
#include <stdint.h>

#include "netconfidence.h"

#define NETPERF_DEFAULT_SERVICE_NAME     "netperf4"
#define NETPERF_DTD_FILE (const xmlChar *)"./netperf_docs.dtd"
#define NETPERF_VERSION (const xmlChar *)"4"
#define NETPERF_UPDATE  (const xmlChar *)"0"
#define NETPERF_FIX     (const xmlChar *)"999"

#define NETPERF_DEBUG_ENTRY(d,w) \
  if (d) { \
    fprintf(w,\
	    "DEBUG entering function %s file %s \n",\
	    __func__, \
	    __FILE__); \
    fflush(w); \
  }

#define NETPERF_DEBUG_EXIT(d,w) \
  if (d) { \
    fprintf(w,\
	    "DEBUG exiting function %s file %s \n",\
	    __func__, \
	    __FILE__); \
    fflush(w); \
  }

/* Macros for accessing fields in the global netperf structures. */
#define SET_TEST_STATE(state)             test->new_state = state
#define GET_TEST_STATE                    test->new_state
#define CHECK_REQ_STATE                   test->state_req 
#define GET_TEST_DATA(test)               test->test_specific_data
#define NO_STATE_CHANGE(test)             (test->state_req == test->new_state)

#ifndef WIN32
#define NETPERF_PATH_SEP "/"
#else
#define NETPERF_PATH_SEP "\\"
#endif

/* a set of server_t's will exist in the netperf process and will
   describe everything that netperf needs to know about a server
   instance (ie remote netserver). */

typedef enum netserver_state {
  NSRV_PREINIT,
  NSRV_CONNECTED,
  NSRV_VERS,
  NSRV_INIT,
  NSRV_WORK,
  NSRV_ERROR
} ns_state_t;

typedef struct server_instance {
  xmlChar          *id;          /* the id of the server instance. used
                                    in searches and as sanity checks  */

  pthread_rwlock_t rwlock;       /* the mutex used to ensure exclusive
                                    access to this servers resources */

  pthread_mutex_t  *lock;        /* the mutex used to ensure exclusive
                                    access to this servers resources */

  xmlNodePtr       node;         /* the xml document node containing the
                                    servers configuration data */

  int              sock;         /* the socket over which we communicate
                                    with the server */

  ns_state_t       state;        /* in what state is this server
                                    presently? */

  ns_state_t       state_req;    /* the state to which the server was last
                                    requested to move */

  int              err_rc;       /* error code received which caused this 
                                    server to enter the NSRV_ERROR state. */

  char            *err_fn;       /* procedure which placed this server into
                                    the NSRV_ERROR state. */

  pthread_t        tid;          /* the posix thread-id of the server
                                    instance within netperf.
                                    Will only be stored in the netperf
                                    process not the netserver process. 
                                    This stems from a pthread_t being
                                    something that is supposed to be
                                    opaque to the user, so there is no
                                    way to know how to communicate it
                                    to another process/host. */

  struct server_instance *next;  /* pointer to the next server instance
                                    in the hash */
} server_t;

#define SERVER_HASH_BUCKETS 4

typedef struct server_hash_elt {
  pthread_mutex_t  hash_lock;
  pthread_cond_t   condition;
  server_t        *server;
} server_hash_t;


/* a set of test_t's will exist in the netperf process and in each
   netserver process.  Each test_t will describe everything that the
   netperf or netserver needs to know about a test instance. */

typedef enum {
  TEST_PREINIT,
  TEST_INIT,
  TEST_IDLE,
  TEST_LOADED,
  TEST_MEASURE,
  TEST_ERROR,
  TEST_DEAD
} test_state_t;


typedef int       *(*TestFunc)(void *test_data);
typedef xmlNodePtr (*TestDecode)(xmlNodePtr statistics);
typedef int        (*TestClear)(void *test_info);
typedef xmlNodePtr (*TestStats)(void *test_data);

#define NETPERF_MAX_TEST_FUNCTION_NAME 64
#define NETPERF_MAX_TEST_LIBRARY_NAME  PATH_MAX

typedef struct test_instance {
  xmlChar   *id;               /* the global test id of this test
                                  instance */

  xmlChar   *server_id;        /* the id of the server controlling
                                  this test. in the netperf process
                                  context, this will be used to find
                                  the proper server_t which will tell
                                  us the control socket we should use
                                  to reach the test instance */

  xmlNodePtr node;             /* the xml document node containing the
                                  test's configuration data */

  uint32_t   debug;            /* should the test routine generate
				  debug output */

  FILE       *where;           /* where that debug output should go */

  uint32_t   state;            /* the state netperf or netserver believes the
                                  test instance to be in at the moment.
                                  only changed by netperf or netserver */

  uint32_t   new_state;        /* the state the test is currently in. 
                                  this field is modified by the test when
                                  it has transitioned to a new state. */

  uint32_t   state_req;        /* the state to which the test has been
                                  requested to transition. this field is 
                                  monitored by the the test thread and
                                  when the field is changed the test takes
                                  action and changes its state. */

  int        err_rc;           /* error code received which caused this 
                                  test to enter the TEST_ERROR state. */

  char      *err_fn;           /* procedure which placed this test into
                                  the TEST_ERROR state. */

  char      *err_str;          /* character string which reports the error  
                                  causing entry to the TEST_ERROR state. */

  int        err_no;           /* The errno returned by the failing syscall */
  
  pthread_t  tid;              /* the posix thread id of the test
                                  instance within the netserver.
                                  Will only be stored in the netserver
                                  process(es) not the netperf
                                  process. this stems from a pthread_t
                                  being something that is supposed to
                                  be opaque to the user, so there is
                                  no way to know how to communicate it
                                  to another process/host. */

  xmlChar   *test_name;        /* the ASCII name of the test being
                                  performed by this test instance. */

  void      *library_handle;   /* the handle passed back by dlopen when
                                  the library containing the test-specific
                                  routines was opened. */

  TestFunc   test_func;        /* the function pointer returned by dlsym
                                  for the test_name function. This function
                                  is launched as a thread to initialize the
                                  test. */
  
  TestClear  test_clear;       /* the function pointer returned by dlsym
                                  for the test_clear function. the function
                                  clears all statistics counters. */
  
  TestStats  test_stats;       /* the function pointer returned by dlsym
                                  for the test_stats function. this function
                                  reads all statistics counters for the test
                                  and returns an xml statistics node for the
                                  test. */
  
  TestDecode test_decode;      /* the function pointer returned by dlsym
                                  for the test_decode function. this function
                                  is called by netperf to decode, accumulate,
                                  and report statistics nodes returned by tests
                                  from this library. */
  
  xmlNodePtr received_stats;   /* a node to which all test_stats received
                                  by netperf from this test are appended as
                                  children */

  xmlNodePtr dependent_data;   /* a pointer to test-specific things to return
                                  to the test which depends on this test */

  xmlNodePtr dependency_data;  /* a pointer to test-specific things returned
                                  by the test on which this test is dependent */

  void *test_specific_data;    /* a pointer to test-specific things -
                                  config settings for the test, places
                                  to store results that sort of
                                  stuff... */

  struct test_instance *next;  /* pointer to the next test instance
                                  in the hash */

} test_t;


#define TEST_HASH_BUCKETS 16
#define TEST_HASH_VALUE(id)  ((atoi((char *)id + 1)) % TEST_HASH_BUCKETS)


typedef struct test_hash_elt {
  pthread_mutex_t  hash_lock;
  pthread_cond_t   condition;
  test_t          *test;
} test_hash_t;


typedef struct test_set_elt {
  xmlChar       *id;           /* the global test id of the test 
                                  pointed to by this test set element */

  struct test_set_elt  *next;  /* pointer to the next test in this test set */

  struct test_set_elt  *prev;  /* pointer to the previous test in this set  */

  test_t        *test;         /* the test structure for this test */

} tset_elt_t;


typedef struct test_set_instance {
  xmlChar       *id;               /* the global test set id of this
                                      test set instance */
 
  xmlChar       *tests_in_set;     /* xml charater string specifying all
                                      test contained within this test set */

  tset_elt_t    *tests;            /* linked list of tests assigned 
                                      to this test set */

  tset_elt_t    *last;             /* last test in the linked list
                                      assigned to this test set */

  int            num_tests;        /* the number of tests linked into
                                      this test set */
  
  uint32_t       debug;            /* should the report generation routine
                                      produce debug output */

  FILE          *where;            /* where that debug output should go */

  void          *report_data;      /* data buffer to hold report generation
                                      specific data between invocations of
                                      the same report generation routine */

  struct test_set_instance *next;  /* pointer to the next test set instance
                                      in the hash */

  confidence_t   confidence;       /* confidence parameters structure */

} tset_t;

typedef void (*GenReport)(tset_t *test_set,char *report_flags,char *outfile);

#define TEST_SET_HASH_BUCKETS 4
#define TEST_SET_HASH_VALUE(id) ((atoi((char *)id + 1)) % TEST_SET_HASH_BUCKETS)


typedef struct test_set_hash_elt {
  pthread_mutex_t  hash_lock;
  pthread_cond_t   condition;
  tset_t          *test_set;
} tset_hash_t;


/* Error codes to be used within Netperf4 */
#define NPE_MIN_ERROR_NUM -1023
enum {
  NPE_MAX_ERROR_NUM = NPE_MIN_ERROR_NUM,   /* -1023 */
  NPE_COMMANDED_TO_EXIT_NETPERF,
  NPE_TEST_SET_NOT_FOUND,
  NPE_BAD_TEST_RANGE,
  NPE_BAD_TEST_ID,
  NPE_SYS_STATS_DROPPED,
  NPE_TEST_STATS_DROPPED,
  NPE_TEST_NOT_FOUND,
  NPE_TEST_FOUND_IN_ERROR_STATE,
  NPE_TEST_INITIALIZED_FAILED,
  NPE_TEST_INIT_FAILED,                       /* -1013 */
  NPE_INIT_TEST_XMLCOPYNODE_FAILED,
  NPE_INIT_TEST_XMLNEWDOC_FAILED,
  NPE_EMPTY_MSG,
  NPE_UNEXPECTED_MSG,
  NPE_ALREADY_CONNECTED,
  NPE_BAD_VERSION,
  NPE_XMLCOPYNODE_FAILED,
  NPE_PTHREAD_MUTEX_INIT_FAILED,
  NPE_PTHREAD_RWLOCK_INIT_FAILED,
  NPE_PTHREAD_COND_WAIT_FAILED,               /* -1003 */
  NPE_PTHREAD_DETACH_FAILED,
  NPE_PTHREAD_CREATE_FAILED,
  NPE_DEPENDENCY_NOT_PRESENT,
  NPE_DEPENDENCY_ERROR,
  NPE_UNKNOWN_FUNCTION_TYPE,
  NPE_FUNC_NAME_TOO_LONG,
  NPE_FUNC_NOT_FOUND,
  NPE_LIBRARY_NOT_LOADED,
  NPE_ADD_TO_EVENT_LIST_FAILED,               /*  -993 */
  NPE_CONNECT_FAILED,
  NPE_MALLOC_FAILED6,
  NPE_MALLOC_FAILED5,
  NPE_MALLOC_FAILED4,
  NPE_MALLOC_FAILED3,
  NPE_MALLOC_FAILED2,
  NPE_MALLOC_FAILED1,
  NPE_REMOTE_CLOSE,
  NPE_SEND_VERSION_XMLNEWNODE_FAILED,         /*  -983 */
  NPE_SEND_VERSION_XMLSETPROP_FAILED,
  NPE_SEND_CTL_MSG_XMLDOCDUMPMEMORY_FAILED,
  NPE_SEND_CTL_MSG_XMLCOPYNODE_FAILED,
  NPE_SEND_CTL_MSG_XMLNEWNODE_FAILED,
  NPE_SEND_CTL_MSG_XMLNEWDTD_FAILED,
  NPE_SEND_CTL_MSG_XMLNEWDOC_FAILED,
  NPE_SEND_CTL_MSG_FAILURE,
  NPE_SHASH_ADD_FAILED,
  NPE_XMLPARSEMEMORY_ERROR,
  NPE_NEG_MSG_BYTES,
  NPE_TIMEOUT,
  NPE_SUCCESS = 0
};

#ifdef NETLIB

const char *NP_ERROR_NAMES[] = {
  "NPE_MAX_ERROR_NUM",
  "NPE_COMMANDED_TO_EXIT_NETPERF",
  "NPE_TEST_SET_NOT_FOUND",
  "NPE_BAD_TEST_RANGE",
  "NPE_BAD_TEST_ID",
  "NPE_SYS_STATS_DROPPED",
  "NPE_TEST_STATS_DROPPED",
  "NPE_TEST_NOT_FOUND",
  "NPE_TEST_FOUND_IN_ERROR_STATE",
  "NPE_TEST_INITIALIZED_FAILED",
  "NPE_TEST_INIT_FAILED",
  "NPE_INIT_TEST_XMLCOPYNODE_FAILED",
  "NPE_INIT_TEST_XMLNEWDOC_FAILED",
  "NPE_EMPTY_MSG",
  "NPE_UNEXPECTED_MSG",
  "NPE_ALREADY_CONNECTED",
  "NPE_BAD_VERSION",
  "NPE_XMLCOPYNODE_FAILED",
  "NPE_PTHREAD_MUTEX_INIT_FAILED",
  "NPE_PTHREAD_RWLOCK_INIT_FAILED",
  "NPE_PTHREAD_COND_WAIT_FAILED",
  "NPE_PTHREAD_DETACH_FAILED",
  "NPE_PTHREAD_CREATE_FAILED",
  "NPE_DEPENDENCY_NOT_PRESENT",
  "NPE_DEPENDENCY_ERROR",
  "NPE_UNKNOWN_FUNCTION_TYPE",
  "NPE_FUNC_NAME_TOO_LONG",
  "NPE_FUNC_NOT_FOUND",
  "NPE_LIBRARY_NOT_LOADED",
  "NPE_ADD_TO_EVENT_LIST_FAILED",
  "NPE_CONNECT_FAILED",
  "NPE_MALLOC_FAILED6",
  "NPE_MALLOC_FAILED5",
  "NPE_MALLOC_FAILED4",
  "NPE_MALLOC_FAILED3",
  "NPE_MALLOC_FAILED2",
  "NPE_MALLOC_FAILED1",
  "NPE_REMOTE_CLOSE",
  "NPE_SEND_VERSION_XMLNEWNODE_FAILED",
  "NPE_SEND_VERSION_XMLSETPROP_FAILED",
  "NPE_SEND_CTL_MSG_XMLDOCDUMPMEMORY_FAILED",
  "NPE_SEND_CTL_MSG_XMLCOPYNODE_FAILED",
  "NPE_SEND_CTL_MSG_XMLNEWNODE_FAILED",
  "NPE_SEND_CTL_MSG_XMLNEWDTD_FAILED",
  "NPE_SEND_CTL_MSG_XMLNEWDOC_FAILED",
  "NPE_SEND_CTL_MSG_FAILURE",
  "NPE_SHASH_ADD_FAILED",
  "NPE_XMLPARSEMEMORY_ERROR",
  "NPE_NEG_MSG_BYTES",
  "NPE_TIMEOUT"
};

#endif
