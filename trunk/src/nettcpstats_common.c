/*
 * PN: New source file to process TCP statistics
 */

/*

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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "netperf.h"

#include "nettcpstats.h"

enum {
  TCP_STATS_MALLOC_FAILED = -3,
  TCP_STATS_REQUESTED_STATE_INVALID = -2,
  TCP_STATS_STATE_CORRUPTED = -1,
  TCP_STATS_SUCCESS = 0
};

static void
update_tcp_stats(test_t *test)
{

  nettcpstat_data_t  *tsd = GET_TEST_DATA(test);

  tcp_stat_counters_t *start	= tsd->starting_tcp_counters;
  tcp_stat_counters_t *end	= tsd->ending_tcp_counters;
  tcp_stat_counters_t *delta	= tsd->delta_tcp_counters;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  delta->tcpLoss	= end->tcpLoss		- start->tcpLoss;
  delta->tcpLostRtxmts	= end->tcpLostRtxmts	- start->tcpLostRtxmts;
  delta->tcpFastRtxmts	= end->tcpFastRtxmts	- start->tcpFastRtxmts;
  delta->tcpTimeouts	= end->tcpTimeouts	- start->tcpTimeouts;

  if (test->debug) {
    fprintf(test->where, "%s: delta tcpLoss = %" PRIu64,
		    __func__, delta->tcpLoss);
    fprintf(test->where, "\tdelta tcpLostRtxmts = %" PRIu64,
		    delta->tcpLostRtxmts);
    fprintf(test->where, "\tdelta tcpFastRtxmts = %" PRIu64,
		    delta->tcpFastRtxmts);
    fprintf(test->where, "\tdelta tcpTimeouts = %" PRIu64,
		    delta->tcpTimeouts);
    fprintf(test->where, "\n");
    fflush(test->where);
  }

  NETPERF_DEBUG_EXIT(test->debug, test->where);

}
  
static void
report_test_failure(test, function, err_code, err_string)
  test_t *test;
  char   *function;
  int     err_code;
  char   *err_string;
{

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  test->err_no    = GET_ERRNO;
  if (test->debug) {
    fprintf(test->where,"%s: called report_test_failure:\n",function);
    fprintf(test->where,"\nreporting  %s  errno = %d\n",err_string,test->err_no);
    fflush(test->where);
  }
  test->err_rc    = err_code;
  test->err_fn    = function;
  test->err_str   = err_string;
  test->new_state = TEST_ERROR;
}

static void
clear_tcp_stat_counters(tcp_stat_counters_t *counter)
{

  if ( NULL != counter) {
    counter->tcpLoss		= 0;
    counter->tcpLostRtxmts	= 0;
    counter->tcpFastRtxmts	= 0;
    counter->tcpTimeouts	= 0;
  }
  else {
    /* Is there a way to log this? */
  }

}

int
tcp_stats_clear_stats(test_t *test)
{

  nettcpstat_data_t *tsd = GET_TEST_DATA(test);

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  if ( NULL != tsd) {
    clear_tcp_stat_counters(tsd->starting_tcp_counters);
    clear_tcp_stat_counters(tsd->ending_tcp_counters);
    clear_tcp_stat_counters(tsd->delta_tcp_counters);
  }

  return(NPE_SUCCESS);
}

static xmlAttrPtr
set_counter_attribute(test_t *test, xmlNodePtr stats,char *name, uint64_t value)
{
  xmlAttrPtr  ap  = NULL;
  char        value_str[32];

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  sprintf(value_str,"%" PRIu64,value); 
  ap = xmlSetProp(stats,(xmlChar *)name,(xmlChar *)value_str);
  if (test->debug) {
    fprintf(test->where,"%s=%s from %"PRIx64"\n",name,value_str,value);
    fflush(test->where);
  }
  return(ap);
}

xmlNodePtr
tcp_stats_get_stats(test_t *test)
{
  xmlNodePtr stats = NULL;
  xmlAttrPtr ap    = NULL;

  nettcpstat_data_t  *tsd = GET_TEST_DATA(test);

  tcp_stat_counters_t *start	= tsd->starting_tcp_counters;
  tcp_stat_counters_t *end	= tsd->ending_tcp_counters;
  tcp_stat_counters_t *delta	= tsd->delta_tcp_counters;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  if (test->debug) {
    fprintf(test->where,"tcp_stats: entering for %s test %s\n",
		    test->id, test->test_name);
    fflush(test->where);
  }

  if ((stats = xmlNewNode(NULL,(xmlChar *)"tcp_stats")) != NULL) {
    /* PN: 06/29/2007
     * set the properites of the tcp_stats message --
     * tid (REQUIRED), and the tcp stat counters.
     */ 
    ap = xmlSetProp(stats,(xmlChar *)"tid",test->id);
    if (ap != NULL) {
      ap = set_counter_attribute(test, stats, "tcp_loss", 
		      delta->tcpLoss);
    }
    if (ap != NULL) {
      ap = set_counter_attribute(test, stats, "tcp_lost_rtxmts", 
		      delta->tcpLostRtxmts);
    }
    if (ap != NULL) {
      ap = set_counter_attribute(test, stats, "tcp_fast_rtxmts", 
		      delta->tcpFastRtxmts);
    }
    if (ap != NULL) {
      ap = set_counter_attribute(test, stats, "tcp_timeouts", 
		      delta->tcpTimeouts);
    }

    if (ap == NULL) {
      xmlFreeNode(stats);
      stats = NULL;
    }
  }

  if (test->debug) {
    fprintf(test->where,"tcp_stats: exiting for %s test %s\n",
		    test->id, test->test_name);
    fflush(test->where);
  }

  return(stats);
}


void
tcp_stats_decode_stats(xmlNodePtr stats, test_t *test)
{
  NETPERF_DEBUG_ENTRY(test->debug,test->where);
}

void
tcp_stats(test_t *test)
{
  
  int err;
  int num_cpus;

  nettcpstat_data_t  *tsd;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  tsd = (nettcpstat_data_t *)malloc(sizeof(nettcpstat_data_t));

  if (NULL == tsd) {
    report_test_failure(test,
                        "tcp_stats",
                         TCP_STATS_MALLOC_FAILED,
                         "call to malloc failed");
  }
  else {

    memset(tsd,0,sizeof(nettcpstat_data_t));

    test->test_specific_data = tsd;

    while ((GET_TEST_STATE != TEST_ERROR) &&
           (GET_TEST_STATE != TEST_DEAD)) {

      switch (GET_TEST_STATE) {

      case TEST_PREINIT:
        err = tcp_stat_init(test);

	if (err) {
	  tcp_stat_clear(test);
	  report_test_failure(test, "tcp_stats", err,
			    "call to tcp_stat_init failed");
	}
	else {

	  tsd->starting_tcp_counters = 
		  (tcp_stat_counters_t *)malloc(sizeof(tcp_stat_counters_t));
	  tsd->ending_tcp_counters = 
		  (tcp_stat_counters_t *)malloc(sizeof(tcp_stat_counters_t));
	  tsd->delta_tcp_counters = 
		  (tcp_stat_counters_t *)malloc(sizeof(tcp_stat_counters_t));

	  if ((NULL != tsd->starting_tcp_counters) &&
	      (NULL != tsd->ending_tcp_counters) &&
	      (NULL != tsd->delta_tcp_counters)) {

	    memset(tsd->starting_tcp_counters, 0, sizeof(tcp_stat_counters_t));
	    memset(tsd->ending_tcp_counters, 0, sizeof(tcp_stat_counters_t));
	    memset(tsd->delta_tcp_counters, 0, sizeof(tcp_stat_counters_t));
	    
	    SET_TEST_STATE(TEST_INIT);
	  }
	  else {
	    if (tsd->starting_tcp_counters) free(tsd->starting_tcp_counters);
	    if (tsd->ending_tcp_counters) free(tsd->ending_tcp_counters);
	    if (tsd->delta_tcp_counters) free(tsd->delta_tcp_counters);

	    report_test_failure(test, "tcp_stats", TCP_STATS_MALLOC_FAILED,
			    "call to malloc failed");
	    
	  }
	}
	break;

	case TEST_INIT:
	  if (test->debug) {
	          fprintf(test->where,"tcp_stats: in INIT state\n");
	          fflush(test->where);
	  }
	  if (CHECK_REQ_STATE == TEST_IDLE) {
	    SET_TEST_STATE(TEST_IDLE);
	  } else {
            report_test_failure(test, "tcp_stats",
		               TCP_STATS_REQUESTED_STATE_INVALID,
                              "tcp_stats in INIT, req to INVALID state");
          }
	  break;

	case TEST_IDLE:
	  if (test->debug) {
	         fprintf(test->where,"tcp_stats: in IDLE state\n");
	         fflush(test->where);
	  }
	  if (CHECK_REQ_STATE == TEST_IDLE) {
	    g_usleep(1000000);
	  } else if (CHECK_REQ_STATE == TEST_LOADED) {
	    SET_TEST_STATE(TEST_LOADED);
	  } else if (CHECK_REQ_STATE == TEST_DEAD) {
	    SET_TEST_STATE(TEST_DEAD);
	  } else {
	    report_test_failure(test, "tcp_stats",
			    TCP_STATS_REQUESTED_STATE_INVALID,
			    "tcp_stats in IDLE, req to INVALID state");
	  }
	  break;

	case TEST_MEASURE:
	  if (test->debug) {
	         fprintf(test->where,"tcp_stats: in MEAS state\n");
	         fflush(test->where);
	  }
	  if (CHECK_REQ_STATE == TEST_MEASURE) {
	    g_usleep(1000000);
	  } else if (CHECK_REQ_STATE == TEST_LOADED) {
	    get_tcp_stat_counters(tsd->ending_tcp_counters, test);
	    update_tcp_stats(test);
	    SET_TEST_STATE(TEST_LOADED);
	    
	  } else {
	    report_test_failure(test, "tcp_stats",
			    TCP_STATS_REQUESTED_STATE_INVALID,
			    "tcp_stats in MEAS, Can transition only to LOAD");
	  }
	  break;
	  
	case TEST_LOADED:
	  if (test->debug) {
	         fprintf(test->where,"tcp_stats: in LOAD state\n");
	         fflush(test->where);
	  }
	  if (CHECK_REQ_STATE == TEST_LOADED) {
	    g_usleep(1000000);
	    
	  } else if (CHECK_REQ_STATE == TEST_MEASURE) {
	    get_tcp_stat_counters(tsd->starting_tcp_counters, test);
	    SET_TEST_STATE(TEST_MEASURE);
	  } else if (CHECK_REQ_STATE == TEST_IDLE) {
	    SET_TEST_STATE(TEST_IDLE);
	  } else {
	    report_test_failure(test, "tcp_stats",
			    TCP_STATS_REQUESTED_STATE_INVALID,
			    "tcp_stats in LOAD, Can transition only to IDLE, MEAS"); 
	  }
	  break;

        default:
	  report_test_failure(test, "tcp_stats", TCP_STATS_STATE_CORRUPTED,
			"tcp_stats found in ILLEGAL state");

      }
    }

    /* do we ever get here? seems that if we do, it would be spinning
     * like crazy?!?  raj 2005-10-06 
     * Adapted, PN: 06/28/2007 */
    while (GET_TEST_STATE != TEST_DEAD) {
      g_usleep(100000);
      if (CHECK_REQ_STATE == TEST_DEAD) {
        SET_TEST_STATE(TEST_DEAD);
      }
    }
    /* Clear of any malloced memoery */
    if (tsd->starting_tcp_counters) free(tsd->starting_tcp_counters);
    if (tsd->ending_tcp_counters) free(tsd->ending_tcp_counters);
    if (tsd->delta_tcp_counters) free(tsd->delta_tcp_counters);
    tcp_stat_clear(test);

  }
}
