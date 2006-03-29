/*

Copyright (c) 2005,2006 Hewlett-Packard Company

$Id$

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

#ifndef _NETPERF_H
#define _NETPERF_H

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

/* still more nested includes to get the uint stuff... */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# else
typedef          int        int32_t;
typedef unsigned int       uint32_t;
typedef          long long  int64_t;
typedef unsigned long long uint64_t;
# endif
#endif

/* under Windows we need to get the magic for "SOCKET" and one of
   these next three does it */
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif

#ifdef WITH_GLIB
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#define NETPERF_MUTEX_T GMutex
#define NETPERF_RWLOCK_T GStaticRWLock
#define NETPERF_THREAD_T GThread *
#define NETPERF_COND_T GCond
#define NETPERF_ABS_TIMESPEC GTimeVal
#define NETPERF_ABS_TIMESET(base,a,b) base.tv_sec = a;base.tv_usec=b;
#define NETPERF_MUTEX_LOCK g_mutex_lock
#define NETPERF_MUTEX_UNLOCK g_mutex_unlock
#define NETPERF_COND_TIMEDWAIT g_cond_timedwait
#define NETPERF_COND_BROADCAST g_cond_broadcast
#define NETPERF_RWLOCK_WRLOCK g_static_rw_lock_writer_lock
#define NETPERF_RWLOCK_WRITER_UNLOCK g_static_rw_lock_writer_unlock
#define NETPERF_STAT g_stat
#define NETPERF_SNPRINTF g_snprintf
#elif defined(HAVE_PTHREAD_H)
#include <pthread.h>
#define NETPERF_MUTEX_T pthread_mutex_t
#define NETPERF_RWLOCK_T pthread_rwlock_t
#define NETPERF_THREAD_T pthread_t
#define NETPERF_COND_T pthread_cond_t
#define NETPERF_ABS_TIMESPEC struct timespec
#define NETPERF_ABS_TIMESET(base,a,b) base.tv_sec = a;base.tv_nsec=b;
#define NETPERF_MUTEX_LOCK pthread_mutex_lock
#define NETPERF_MUTEX_UNLOCK pthread_mutex_unlock
#define NETPERF_COND_TIMEDWAIT pthread_cond_timedwait
#define NETPERF_COND_BROADCAST pthread_cond_broadcast
#define NETPERF_RWLOCK_WRLOCK pthread_rwlock_wrlock
#define NETPERF_RWLOCK_WRITER_UNLOCK pthread_rwlock_unlock
#define NETPERF_STAT stat
#define NETPERF_SNPRINTF snprintf
#else
#error Netperf4 requires either glib or pthreads
#endif

#define NETPERF_RING_BUFFER_STRING "netperf4 ring data"

#include "netperf_hist.h"

#ifdef WIN32
#define NETPERF_DEBUG_LOG_DIR "c:\\temp\\"
#define NETPERF_DEBUG_LOG_PREFIX  "netperf"
#define NETPERF_DEBUG_LOG_SUFFIX  ".log"
#define netperf_socklen_t socklen_t
#define CLOSE_SOCKET(x) closesocket(x)
#define strcasecmp(a,b) _stricmp(a,b)
#define getpid() ((int)GetCurrentProcessId())
#define __func__ __FUNCTION__
#define PATH_MAX MAXPATHLEN
#else
#define NETPERF_DEBUG_LOG_DIR "/tmp/"
#define NETPERF_DEBUG_LOG_PREFIX  "netperf"
#define NETPERF_DEBUG_LOG_SUFFIX  ".log"
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SOCKET int
#define CLOSE_SOCKET(x) close(x)
#define GET_ERRNO errno
#endif

#include "netconfidence.h"

#define NETPERF_MESSAGE_HEADER_SIZE sizeof(guint32)

#define NETPERF_DEFAULT_SERVICE_NAME     "netperf4"
#define NETPERF_DTD_FILE (const xmlChar *)"http://www.netperf.org/netperf_docs.dtd/1.0"
#define NETPERF_VERSION (const xmlChar *)"4"
#define NETPERF_UPDATE  (const xmlChar *)"0"
#define NETPERF_FIX     (const xmlChar *)"997"

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
#define SET_TEST_DATA(test,ptr)           test->test_specific_data = ptr


#ifdef WIN32
#define CHECK_FOR_NOT_SOCKET (WSAGetLastError() == WSAENOTSOCK)
#define CHECK_FOR_INVALID_SOCKET (temp_socket == INVALID_SOCKET)
#define CHECK_FOR_RECV_ERROR(len) (len == SOCKET_ERROR)
#define CHECK_FOR_SEND_ERROR(len) (len >= 0) || \
        (len == SOCKET_ERROR && WSAGetLastError() == WSAEINTR)
#define GET_ERRNO WSAGetLastError()
#define NETPERF_PATH_SEP "\\"
#ifndef SHUT_WR
#define SHUT_WR SD_SEND
#endif
#else
#define CHECK_FOR_NOT_SOCKET (errno == ENOTSOCK)
#define CHECK_FOR_INVALID_SOCKET (temp_socket < 0)
#define CHECK_FOR_RECV_ERROR(len) (len < 0)
#define CHECK_FOR_SEND_ERROR(len) (len >=0) || (errno == EINTR)
#define GET_ERRNO errno
#define NETPERF_PATH_SEP "/"
#endif


#ifdef HAVE_GETHRTIME
#define NETPERF_TIMESTAMP_T hrtime_t
#else
#define NETPERF_TIMESTAMP_T struct timeval
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
  NSRV_ERROR,
  NSRV_CLOSE,
  NSRV_EXIT
} ns_state_t;

typedef struct server_instance {
  xmlChar          *id;          /* the id of the server instance. used
                                    in searches and as sanity checks  */

  xmlChar          *my_nid;      /* used in netserver, used to be
				    global */

  NETPERF_RWLOCK_T rwlock;       /* the mutex used to ensure exclusive
                                    access to this servers resources */

  NETPERF_MUTEX_T  *lock;        /* the mutex used to ensure exclusive
                                    access to this servers resources */

  xmlNodePtr       node;         /* the xml document node containing the
                                    servers configuration data */

  SOCKET           sock;         /* the socket over which we communicate
                                    with the server */

  GIOChannel       *source;      /* the control channel over which we
				    communicate with the server */

  ns_state_t       state;        /* in what state is this server
                                    presently? */

  ns_state_t       state_req;    /* the state to which the server was last
                                    requested to move */

  int              err_rc;       /* error code received which caused this 
                                    server to enter the NSRV_ERROR state. */

  char            *err_fn;       /* procedure which placed this server into
                                    the NSRV_ERROR state. */

  NETPERF_THREAD_T thread_id;    /* the posix thread-id of the server
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
  NETPERF_MUTEX_T  *hash_lock;
  NETPERF_COND_T   *condition;
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
  
  NETPERF_THREAD_T thread_id;   /* as the different threads packages
				   have differnt concepts of what the
				   opaque thread id should be we will
				   have to do some interesting
				   backflips to deal with them */

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
  NETPERF_MUTEX_T  *hash_lock;
  NETPERF_COND_T   *condition;
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
  NETPERF_MUTEX_T  *hash_lock;
  NETPERF_COND_T   *condition;
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
  NPE_SET_THREAD_LOCALITY_FAILED,
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
  "NPE_TIMEOUT",
  "NPE_SET_THREAD_LOCALITY_FAILED"
};

#endif
#endif
