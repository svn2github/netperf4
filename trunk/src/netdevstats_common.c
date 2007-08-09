/* 
 * PN: New source file to process network device statistics
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

#include "netdevstats.h"

enum {
  NETDEV_STATS_INTERFACE_MISMATCH = 4,
  NETDEV_STATS_MALLOC_FAILED = -3,
  NETDEV_STATS_REQUESTED_STATE_INVALID = -2,
  NETDEV_STATS_STATE_CORRUPTED = -1,
  NETDEV_STATS_SUCCESS = 0
};

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
update_netdev_stats(test_t *test)
{

  int i;
  netdevstat_data_t  *tsd = GET_TEST_DATA(test);

  netdev_stat_counters_t *start	= tsd->starting_netdev_counters;
  netdev_stat_counters_t *end	= tsd->ending_netdev_counters;
  netdev_stat_counters_t *delta	= tsd->delta_netdev_counters;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  for (i = 0; i < tsd->num_network_interfaces; i++) {

    if (strcmp(end[i].devName, start[i].devName)) {
      report_test_failure(test,
                         "netdev_stats",
                          NETDEV_STATS_INTERFACE_MISMATCH,
                         "Interface names mismatch");
    }

    strcpy(delta[i].devName, end[i].devName);
    delta[i].rcvPackets	   = end[i].rcvPackets	  - start[i].rcvPackets;
    delta[i].rcvErrors	   = end[i].rcvErrors	  - start[i].rcvErrors;
    delta[i].rcvDrops	   = end[i].rcvDrops	  - start[i].rcvDrops;
    delta[i].trxPackets	   = end[i].trxPackets	  - start[i].trxPackets;
    delta[i].trxErrors	   = end[i].trxErrors	  - start[i].trxErrors;
    delta[i].trxDrops	   = end[i].trxDrops	  - start[i].trxDrops;
    delta[i].trxCollisions = end[i].trxCollisions - start[i].trxCollisions;

    if (test->debug) {
      fprintf(test->where, "Interface: %d, devName = %s", 
	      	     i, delta[i].devName);
      fprintf(test->where, "\tdelta rcvPackets = %" PRIu64,
		    delta[i].rcvPackets);
      fprintf(test->where, "\tdelta rcvErrors = %" PRIu64,
		    delta[i].rcvErrors);
      fprintf(test->where, "\tdelta rcvDrops = %" PRIu64,
		    delta[i].rcvDrops);
      fprintf(test->where, "\tdelta trxPackets = %" PRIu64,
		    delta[i].trxPackets);
      fprintf(test->where, "\tdelta trxErrors = %" PRIu64,
		    delta[i].trxErrors);
      fprintf(test->where, "\tdelta trxDrops = %" PRIu64,
		    delta[i].trxDrops);
      fprintf(test->where, "\tdelta trxCollisions = %" PRIu64,
		    delta[i].trxCollisions);
      fprintf(test->where, "\n");
      fflush(test->where);
   }

  }

  NETPERF_DEBUG_EXIT(test->debug, test->where);

}
  
static void
clear_netdev_stat_counters(netdev_stat_counters_t *counters, int count)
{

  int i;

  if ( NULL != counters) {
    for (i=0; i<count; i++) {

      strcpy(counters[i].devName, "");
      counters[i].rcvPackets		= 0;
      counters[i].rcvErrors		= 0;
      counters[i].rcvDrops		= 0;
      counters[i].trxPackets		= 0;
      counters[i].trxErrors		= 0;
      counters[i].trxDrops		= 0;
      counters[i].trxCollisions		= 0;

    }
  }
  else {
    /* Is there a way to log this? */
  }

}

int
netdev_stats_clear_stats(test_t *test)
{

  netdevstat_data_t *tsd = GET_TEST_DATA(test);

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  if ( NULL != tsd) {
    clear_netdev_stat_counters(tsd->starting_netdev_counters,
		    tsd->num_network_interfaces);
    clear_netdev_stat_counters(tsd->ending_netdev_counters,
		    tsd->num_network_interfaces);
    clear_netdev_stat_counters(tsd->delta_netdev_counters,
		    tsd->num_network_interfaces);
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
netdev_stats_get_stats(test_t *test)
{
  xmlNodePtr stats = NULL;
  xmlAttrPtr ap    = NULL;
  xmlNodePtr interface_stats = NULL;
  int i = 0;

  netdevstat_data_t  *tsd = GET_TEST_DATA(test);

  netdev_stat_counters_t *delta	= tsd->delta_netdev_counters;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  if (test->debug) {
    fprintf(test->where,"netdev_stats: entering for %s test %s\n",
		    test->id, test->test_name);
    fflush(test->where);
  }

  if ((stats = xmlNewNode(NULL,(xmlChar *)"netdev_stats")) != NULL) {
    /* set the properites of the netdev_stats message --
     * tid (REQUIRED).
     */ 
    ap = xmlSetProp(stats,(xmlChar *)"tid",test->id);

    if (ap != NULL) {
      ap = set_counter_attribute(test, stats, "number_interfaces", 
		      tsd->num_network_interfaces);
    }

    for (i = 0; i < tsd->num_network_interfaces; i++) {
      if ((interface_stats = 
	   xmlNewNode(NULL,(xmlChar *)"per_interface_stats")) != NULL) {

          /* set the properties for each interface */
          ap = xmlSetProp(interface_stats, (xmlChar *)"interface_name",
		      (xmlChar *)delta[i].devName);
    
          if (ap != NULL) {
            ap = set_counter_attribute(test, interface_stats, "rcv_packets", 
		      delta[i].rcvPackets);
          }
          if (ap != NULL) {
            ap = set_counter_attribute(test, interface_stats, "rcv_errors", 
	 	      delta[i].rcvErrors);
          }
          if (ap != NULL) {
            ap = set_counter_attribute(test, interface_stats, "rcv_drops", 
		      delta[i].rcvDrops);
          }
          if (ap != NULL) {
            ap = set_counter_attribute(test, interface_stats, "trx_packets", 
		      delta[i].trxPackets);
          }
          if (ap != NULL) {
            ap = set_counter_attribute(test, interface_stats, "trx_errors", 
		      delta[i].trxErrors);
          }
          if (ap != NULL) {
            ap = set_counter_attribute(test, interface_stats, "trx_drops", 
		      delta[i].trxDrops);
          }
          if (ap != NULL) {
            ap = set_counter_attribute(test, interface_stats, "trx_collisions", 
		      delta[i].trxCollisions);
          }
          xmlAddChild(stats, interface_stats);

      } else {
	  /* error xmlNewNode failed to allocate new node for interface */
	  ap = NULL;
	  break; /* from the loop */
      }
    }

    if (ap == NULL) {
	xmlFreeNode(stats);
	stats = NULL;
    }

  }

  if (test->debug) {
    fprintf(test->where,"netdev_stats: exiting for %s test %s\n",
		    test->id, test->test_name);
    fflush(test->where);
  }

  return(stats);
}

void
netdev_stats_decode_stats(xmlNodePtr stats, test_t *test)
{
  NETPERF_DEBUG_ENTRY(test->debug,test->where);
}

void
netdev_stats(test_t *test)
{
  
  int err;
  int num_interfaces;

  netdevstat_data_t  *tsd;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  tsd = (netdevstat_data_t *)malloc(sizeof(netdevstat_data_t));


  if (NULL == tsd) {
    report_test_failure(test,
                        "netdev_stats",
                         NETDEV_STATS_MALLOC_FAILED,
                         "call to malloc failed");
  }
  else {

    memset(tsd,0,sizeof(netdevstat_data_t));

    test->test_specific_data = tsd;

    while ((GET_TEST_STATE != TEST_ERROR) &&
           (GET_TEST_STATE != TEST_DEAD)) {

      switch (GET_TEST_STATE) {

      case TEST_PREINIT:
        err = netdev_stat_init(test);

	if (err) {
	  netdev_stat_clear(test);
	  report_test_failure(test, "netdev_stats", err,
			    "call to netdev_stat_init failed");
	}
	else {
	  num_interfaces = tsd->num_network_interfaces;

	  tsd->starting_netdev_counters = 
		  (netdev_stat_counters_t *)malloc(num_interfaces *
						   sizeof(netdev_stat_counters_t));
	  tsd->ending_netdev_counters = 
		  (netdev_stat_counters_t *)malloc(num_interfaces *
						   sizeof(netdev_stat_counters_t));
	  tsd->delta_netdev_counters = 
		  (netdev_stat_counters_t *)malloc(num_interfaces *
						   sizeof(netdev_stat_counters_t));

	  if ((NULL != tsd->starting_netdev_counters) &&
	      (NULL != tsd->ending_netdev_counters) &&
	      (NULL != tsd->delta_netdev_counters)) {

	    memset(tsd->starting_netdev_counters, 0, num_interfaces *
			    sizeof(netdev_stat_counters_t));
	    memset(tsd->ending_netdev_counters, 0, num_interfaces *
			    sizeof(netdev_stat_counters_t));
	    memset(tsd->delta_netdev_counters, 0, num_interfaces *
			    sizeof(netdev_stat_counters_t));
	    
	    SET_TEST_STATE(TEST_INIT);
	  }
	  else {
	    if (tsd->starting_netdev_counters) free(tsd->starting_netdev_counters);
	    if (tsd->ending_netdev_counters) free(tsd->ending_netdev_counters);
	    if (tsd->delta_netdev_counters) free(tsd->delta_netdev_counters);

	    report_test_failure(test, "netdev_stats", NETDEV_STATS_MALLOC_FAILED,
			    "call to malloc failed");
	    
	  }
	}
	break;

	case TEST_INIT:
	  if (test->debug) {
	          fprintf(test->where,"netdev_stats: in INIT state\n");
	          fflush(test->where);
	  }
	  if (CHECK_REQ_STATE == TEST_IDLE) {
	    SET_TEST_STATE(TEST_IDLE);
	  } else {
            report_test_failure(test, "netdev_stats",
		               NETDEV_STATS_REQUESTED_STATE_INVALID,
                              "netdev_stats in INIT, req to INVALID state");
          }
	  break;

	case TEST_IDLE:
	  if (test->debug) {
	         fprintf(test->where,"netdev_stats: in IDLE state\n");
	         fflush(test->where);
	  }
	  if (CHECK_REQ_STATE == TEST_IDLE) {
	    g_usleep(1000000);
	  } else if (CHECK_REQ_STATE == TEST_LOADED) {
	    SET_TEST_STATE(TEST_LOADED);
	  } else if (CHECK_REQ_STATE == TEST_DEAD) {
	    SET_TEST_STATE(TEST_DEAD);
	  } else {
	    report_test_failure(test, "netdev_stats",
			    NETDEV_STATS_REQUESTED_STATE_INVALID,
			    "netdev_stats in IDLE, req to INVALID state");
	  }
	  break;

	case TEST_MEASURE:
	  if (test->debug) {
	         fprintf(test->where,"netdev_stats: in MEAS state\n");
	         fflush(test->where);
	  }
	  if (CHECK_REQ_STATE == TEST_MEASURE) {
	    g_usleep(1000000);
	  } else if (CHECK_REQ_STATE == TEST_LOADED) {
	    get_netdev_stat_counters(tsd->ending_netdev_counters, test);
	    update_netdev_stats(test);
	    SET_TEST_STATE(TEST_LOADED);
	    
	  } else {
	    report_test_failure(test, "netdev_stats",
			    NETDEV_STATS_REQUESTED_STATE_INVALID,
			    "netdev_stats in MEAS, Can transition only to LOAD");
	  }
	  break;
	  
	case TEST_LOADED:
	  if (test->debug) {
	         fprintf(test->where,"netdev_stats: in LOAD state\n");
	         fflush(test->where);
	  }
	  if (CHECK_REQ_STATE == TEST_LOADED) {
	    g_usleep(1000000);
	    
	  } else if (CHECK_REQ_STATE == TEST_MEASURE) {
	    get_netdev_stat_counters(tsd->starting_netdev_counters, test); 
	    SET_TEST_STATE(TEST_MEASURE);
	  } else if (CHECK_REQ_STATE == TEST_IDLE) {
	    SET_TEST_STATE(TEST_IDLE);
	  } else {
	    report_test_failure(test, "netdev_stats",
			    NETDEV_STATS_REQUESTED_STATE_INVALID,
			    "netdev_stats in LOAD, Can transition only to IDLE, MEAS"); 
	  }
	  break;

        default:
	  report_test_failure(test, "netdev_stats", NETDEV_STATS_STATE_CORRUPTED,
			"netdev_stats found in ILLEGAL state");

      }
    }

    /* do we ever get here? seems that if we do, it would be spinning
     * like crazy?!?  raj 2005-10-06 
     * Adapted, PN: 07/05/2007 */
    while (GET_TEST_STATE != TEST_DEAD) {
      g_usleep(100000);
      if (CHECK_REQ_STATE == TEST_DEAD) {
        SET_TEST_STATE(TEST_DEAD);
      }
    }
    /* Clear of any malloced memoery */
    if (tsd->starting_netdev_counters) free(tsd->starting_netdev_counters);
    if (tsd->ending_netdev_counters) free(tsd->ending_netdev_counters);
    if (tsd->delta_netdev_counters) free(tsd->delta_netdev_counters);
    netdev_stat_clear(test);
  }
}
