char netsysstats_id[]="\
@(#)netsysstats_common.c (c) Copyright 2005, Hewlett-Packard Company, $Id$";

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

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "netperf.h"

#include "netsysstats.h"

/* variables are kept in the test_specific data section of the test stucture.
   Only one instant of the test should be running on any system... sgb
   2005-10-15 */


enum {
  SYS_STATS_PSTAT_GETPROCESSOR_FAILED = -5,
  SYS_STATS_MALLOC_FAILED = -4,
  SYS_STATS_GETDYNAMIC_FAILED = -3,
  SYS_STATS_REQUESTED_STATE_INVALID = -2,
  SYS_STATS_STATE_CORRUPTED = -1,
  SYS_STATS_SUCCESS = 0
};


static void
update_sys_stats(test_t *test)
{

  netsysstat_data_t *tsd = GET_TEST_DATA(test);

  int        i;
  uint64_t   subtotal;

  struct timeval *prev  = &(tsd->prev_time);
  struct timeval *curr  = &(tsd->curr_time);
  struct timeval *dtime = &(tsd->delta_elapsed_time);
  struct timeval *ttime = &(tsd->total_elapsed_time);
  
  cpu_time_counters_t *sys_total = tsd->total_sys_counters;
  cpu_time_counters_t *starting  = tsd->starting_cpu_counters;
  cpu_time_counters_t *ending    = tsd->ending_cpu_counters;
  cpu_time_counters_t *delta     = tsd->delta_cpu_counters;
  cpu_time_counters_t *total     = tsd->total_cpu_counters;

  dtime->tv_usec = curr->tv_usec - prev->tv_usec;
  dtime->tv_sec  = curr->tv_sec  - prev->tv_sec;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  if (test->debug) {
    fprintf(test->where,"\tdelta_sec  = %d\t%d\t%d\n",
            (int)dtime->tv_sec,(int)curr->tv_sec,(int)prev->tv_sec);
    fprintf(test->where,"\tdelta_usec = %d\t%d\t%d\n",
            (int)dtime->tv_usec,(int)curr->tv_usec,(int)prev->tv_usec);
    fflush(test->where);
  }

  if (curr->tv_usec < prev->tv_usec) {
    dtime->tv_usec += 1000000;
    dtime->tv_sec--;
  }

  if (dtime->tv_usec >= 1000000) {
    dtime->tv_usec -= 1000000;
    dtime->tv_sec++;
  }

  if (test->debug) {
    fprintf(test->where,"\tdelta_sec  = %d\t%d\t%d\n",
            (int)dtime->tv_sec,(int)curr->tv_sec,(int)prev->tv_sec);
    fprintf(test->where,"\tdelta_usec = %d\t%d\t%d\n",
            (int)dtime->tv_usec,(int)curr->tv_usec,(int)prev->tv_usec);
    fflush(test->where);
  }

  if (test->debug) {
    fprintf(test->where,"\tttime = %d.%d\n",(int)ttime->tv_sec,(int)ttime->tv_usec);
    fflush(test->where);
  }

  ttime->tv_sec += dtime->tv_sec;
  ttime->tv_usec += dtime->tv_usec;

  if (ttime->tv_usec >= 1000000) {
    ttime->tv_usec -= 1000000;
    ttime->tv_sec++;
  }

  if (test->debug) {
    fprintf(test->where,"\tttime = %d.%d\n",(int)ttime->tv_sec,(int)ttime->tv_usec);
    fflush(test->where);
  }

  for (i = 0; i < tsd->num_cpus; i++) {
    /* unsigned 64bit arthmatic should produce the correct answer
       even on counter wrap if all counters are truely unsigned 64 bits.
       sgb  2005-10-15 */
    delta[i].calibrate = ending[i].calibrate - starting[i].calibrate;
    delta[i].idle      = ending[i].idle      - starting[i].idle;
    delta[i].user      = ending[i].user      - starting[i].user;
    delta[i].kernel    = ending[i].kernel    - starting[i].kernel;
    delta[i].interrupt = ending[i].interrupt - starting[i].interrupt;

    subtotal  = delta[i].idle;
    subtotal += delta[i].user;
    subtotal += delta[i].kernel;
    subtotal += delta[i].interrupt;

    delta[i].other = delta[i].calibrate - subtotal;

    if(test->debug) {
      fprintf(test->where, 
	      "%s delta calibrate[%d] = %"PRIx64,
	      __func__, i, delta[i].calibrate);
      fprintf(test->where, 
	      "\n\tdelta idle[%d] = %"PRIx64,
	      i, delta[i].idle);
      fprintf(test->where,
	      "\n\tdelta user[%d] = %"PRIx64,
	      i, delta[i].user);
      fprintf(test->where,
	      "\n\tdelta kern[%d] = %"PRIx64,
	      i, delta[i].kernel);
      fprintf(test->where,
	      "\n\tdelta intr[%d] = %"PRIx64,
	      i, delta[i].interrupt);
      fprintf(test->where,
	      "\n\tdelta othr[%d] = %"PRIx64,
	      i, delta[i].other);
      fprintf(test->where, "\n");
      fflush(test->where);
    }

    if (subtotal > delta[i].calibrate) {
      /* hmm, what is the right thing to do here? for now simply
	 zero-out other. in the future consider emitting a
	 warning. raj 2005-10-06 */
      /* how about setting  calibrate to total and zero-out other.
         this means that percents will never exceed 100%.
         sgb 2005-10-15 */
      delta[i].other = 0;
    }
    
    total[i].calibrate += delta[i].calibrate;
    total[i].idle      += delta[i].idle;
    total[i].user      += delta[i].user;
    total[i].kernel    += delta[i].kernel;
    total[i].interrupt += delta[i].interrupt;
    total[i].other     += delta[i].other;

    sys_total->calibrate += delta[i].calibrate;
    sys_total->idle      += delta[i].idle;
    sys_total->user      += delta[i].user;
    sys_total->kernel    += delta[i].kernel;
    sys_total->interrupt += delta[i].interrupt;
    sys_total->other     += delta[i].other;
  }
  NETPERF_DEBUG_EXIT(test->debug,test->where);
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
clear_cpu_time_counters(cpu_time_counters_t *counters, int count) {
  int i;

  /* lets be paranoid */
  if (NULL != counters) {
    for (i=0;i<count;i++) {
      counters[i].calibrate = 0;
      counters[i].idle = 0;
      counters[i].user = 0;
      counters[i].kernel = 0;
      counters[i].interrupt = 0;
      counters[i].other = 0;
    }
  }
  else {
    /* probably aught to report something here */
  }
}

int
sys_stats_clear_stats(test_t *test)
{
  netsysstat_data_t *tsd = GET_TEST_DATA(test);

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  tsd->total_elapsed_time.tv_usec = 0;
  tsd->total_elapsed_time.tv_sec  = 0;
  tsd->delta_elapsed_time.tv_usec = 0;
  tsd->delta_elapsed_time.tv_sec  = 0;

  clear_cpu_time_counters(tsd->total_sys_counters,1);
  clear_cpu_time_counters(tsd->starting_cpu_counters,tsd->num_cpus);
  clear_cpu_time_counters(tsd->ending_cpu_counters,tsd->num_cpus);
  clear_cpu_time_counters(tsd->total_cpu_counters,tsd->num_cpus);
  clear_cpu_time_counters(tsd->delta_cpu_counters,tsd->num_cpus);

  gettimeofday(&(tsd->prev_time), NULL);
  tsd->curr_time  = tsd->prev_time;
  return(NPE_SUCCESS);
}

static xmlAttrPtr
set_stat_attribute(test_t *test, xmlNodePtr stats,char *name, uint32_t value)
{
  xmlAttrPtr  ap  = NULL;
  char        value_str[32];

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  sprintf(value_str,"%d",value);
  ap = xmlSetProp(stats,(xmlChar *)name,(xmlChar *)value_str);
  if (test->debug) {
    fprintf(test->where,"%s=%s\n",name,value_str);
    fflush(test->where);
  }
  return(ap);
}

static xmlAttrPtr
set_counter_attribute(test_t *test, xmlNodePtr stats,char *name, uint64_t value)
{
  xmlAttrPtr  ap  = NULL;
  char        value_str[32];

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  sprintf(value_str,"%#"PRIx64,value);
  ap = xmlSetProp(stats,(xmlChar *)name,(xmlChar *)value_str);
  if (test->debug) {
    fprintf(test->where,"%s=%s from %"PRIx64"\n",name,value_str,value);
    fflush(test->where);
  }
  return(ap);
}

/* NOTE - we need to figure-out where/when to free the attributes
   returned by set_*_attribute otherwise we have a memory leak. what I
   don't know is if the attribute pointers can/should be freed here or
   if they become part of the xmlNode... */

static xmlAttrPtr
add_per_cpu_attributes(test_t *test,
		       xmlNodePtr stats,
                       cpu_time_counters_t *cpu,
                       uint32_t num_cpus)
{
  uint32_t        i;
  xmlNodePtr cpu_stats = NULL;
  xmlAttrPtr ap        = NULL;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);
  
  for (i = 0; i < num_cpus; i++) {
    if ((cpu_stats = xmlNewNode(NULL,(xmlChar *)"per_cpu_stats")) != NULL) {
      /* set the properites of the per_cpu_stats -
         the cpu_id counter values  sgb 2005-10-17 */
      ap = set_stat_attribute(test, cpu_stats,"cpu_id",i);
      if (ap != NULL) {
        ap = set_counter_attribute(test, cpu_stats, "calibration", cpu[i].calibrate);
      }
      if (ap != NULL) {
        ap = set_counter_attribute(test, cpu_stats, "idle_count",  cpu[i].idle);
      }
#ifndef EXTRA_COUNTERS_MISSING
      if (ap != NULL) {
        ap = set_counter_attribute(test, cpu_stats, "user_count",  cpu[i].user);
      }
      if (ap != NULL) {
        ap = set_counter_attribute(test, cpu_stats, "sys_count",  cpu[i].kernel);
      }
      if (ap != NULL) {
        ap = set_counter_attribute(test, cpu_stats, "int_count",  cpu[i].interrupt);
      }
      if (ap != NULL) {
        ap = set_counter_attribute(test, cpu_stats, "other_count", cpu[i].other);
      }
#endif
      xmlAddChild(stats,cpu_stats);
    } else {
      /* error xmlNewNode failed to allocate a new node for the next cpu */
      ap = NULL;
      break;
    }
  }
  return(ap);
}

xmlNodePtr
sys_stats_get_stats(test_t *test)
{
  xmlNodePtr stats = NULL;
  xmlAttrPtr ap    = NULL;
  
  netsysstat_data_t    *tsd = GET_TEST_DATA(test);
  cpu_time_counters_t  *total_sys  = tsd->total_sys_counters;
  cpu_time_counters_t  *total_cpu  = tsd->total_cpu_counters;
  cpu_time_counters_t  *ending_cpu = tsd->ending_cpu_counters;
  
  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  if (test->debug) {
    fprintf(test->where,"sys_stats_get_stats: entering for %s test %s\n",
            test->id, test->test_name);
    fflush(test->where);
  }

  if ((stats = xmlNewNode(NULL,(xmlChar *)"sys_stats")) != NULL) {
    /* set the properites of the sys_stats message -
       the tid and time stamps/values and counter values  sgb 2004-10-27 */

    ap = xmlSetProp(stats,(xmlChar *)"tid",test->id);
    if (GET_TEST_STATE == TEST_MEASURE) {
      /* get_cpu_time_counters sets current timestamp */
      get_cpu_time_counters(tsd->ending_cpu_counters, &(tsd->curr_time), test);
      if (ap != NULL) {
        ap = set_stat_attribute(test, stats,"time_sec",tsd->curr_time.tv_sec);
      }
      if (ap != NULL) {
        ap = set_stat_attribute(test, stats,"time_usec",tsd->curr_time.tv_usec);
      }
      if (ap != NULL) {
        ap = set_stat_attribute(test, stats,"number_cpus",tsd->num_cpus);
      }
      if (ap != NULL) {
        ap = add_per_cpu_attributes(test, stats, ending_cpu, tsd->num_cpus);
      }
    } else {
      if (ap != NULL) {
        ap = set_stat_attribute(test, stats,
                                "elapsed_sec",
                                tsd->total_elapsed_time.tv_sec);
      }
      if (ap != NULL) {
        ap = set_stat_attribute(test, stats,
                                "elapsed_usec",
                                tsd->total_elapsed_time.tv_usec);
      }
      if (ap != NULL) {
        ap = set_stat_attribute(test, stats, "number_cpus", tsd->num_cpus);
      }
      if (ap != NULL) {
        ap = set_counter_attribute(test, stats, "calibration", total_sys->calibrate);
      }
      if (ap != NULL) {
        ap = set_counter_attribute(test, stats, "idle_count",  total_sys->idle);
      }
#ifndef EXTRA_COUNTERS_MISSING
      if (ap != NULL) {
        ap = set_counter_attribute(test, stats, "user_count",  total_sys->user);
      }
      if (ap != NULL) {
        ap = set_counter_attribute(test, stats, "sys_count",  total_sys->kernel);
      }
      if (ap != NULL) {
        ap = set_counter_attribute(test, stats, "int_count",  total_sys->interrupt);
      }
      if (ap != NULL) {
        ap = set_counter_attribute(test, stats, "other_count", total_sys->other);
      }
#endif
      ap = add_per_cpu_attributes(test,stats,total_cpu, tsd->num_cpus);
    }
    if (ap == NULL) {
      xmlFreeNode(stats);
      stats = NULL;
    }
  }
  if (test->debug) {
    fprintf(test->where,"sys_stats_get_stats: exiting for %s test %s\n",
            test->id, test->test_name);
    fflush(test->where);
  }
  return(stats);
}


void
sys_stats_decode_stats(xmlNodePtr stats, test_t *test)
{
  NETPERF_DEBUG_ENTRY(test->debug,test->where);
}

void
sys_stats(test_t *test)
{
  
  int err;
  int num_cpus;
 
  netsysstat_data_t  *tsd;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  tsd = (netsysstat_data_t *)malloc(sizeof(netsysstat_data_t));

  if (NULL == tsd) {
    report_test_failure(test,
                        "sys_stats",
                        SYS_STATS_MALLOC_FAILED,
                        "call to malloc failed");
  }
  else {
    /* lets make sure we have a valid pointer before we go calling
       memset, so put this after the if (tsd == NULL) bit... */
    memset(tsd,0,sizeof(netsysstat_data_t));
    test->test_specific_data = tsd;
    while ((GET_TEST_STATE != TEST_ERROR) &&
	   (GET_TEST_STATE != TEST_DEAD)) {
      switch (GET_TEST_STATE) {
      case TEST_PREINIT:
	/* following allocates and initializes num_cpus, and psd
	   in the netsysstat_data structure   sgb 2005-10-17 */
	err = sys_cpu_util_init(test);
	num_cpus = tsd->num_cpus;
	if (num_cpus > 0) {
	  if (test->debug) {
	    fprintf(test->where,
		    "sys_stats: allocating counters for %d cpus\n",
		    num_cpus);
	    fflush(test->where);
	  }
	  tsd->total_sys_counters =
	    (cpu_time_counters_t *)malloc(sizeof(cpu_time_counters_t));
	  
	  tsd->starting_cpu_counters = 
	    (cpu_time_counters_t *)malloc(num_cpus * 
					  sizeof(cpu_time_counters_t));
	  tsd->ending_cpu_counters = 
	    (cpu_time_counters_t *)malloc(num_cpus * 
					  sizeof(cpu_time_counters_t));
	  tsd->delta_cpu_counters = 
	    (cpu_time_counters_t *)malloc(num_cpus * 
					  sizeof(cpu_time_counters_t));
	  tsd->total_cpu_counters = 
	    (cpu_time_counters_t *)malloc(num_cpus * 
					  sizeof(cpu_time_counters_t));
	  
	  if ((NULL != tsd->total_sys_counters) && 
	      (NULL != tsd->starting_cpu_counters) &&
	      (NULL != tsd->ending_cpu_counters) &&
	      (NULL != tsd->delta_cpu_counters) &&
	      (NULL != tsd->total_cpu_counters)) {
	    /* put the memset's here, once we know that the pointers are
	       good. raj 2005-10-27
	       for good measure, make sure we are doing a memset for
	       the proper size of each of these blessed things! raj
	       2006-01-23
	       and do it for all the counters just to make sure that
	       stuff like valgrind remains happy. raj 2006-02-29 */

	    memset(tsd->total_sys_counters,0,sizeof(cpu_time_counters_t));
	    memset(tsd->total_cpu_counters,0,
		   (num_cpus * sizeof(cpu_time_counters_t)));
	    memset(tsd->starting_cpu_counters,0,
		   (num_cpus * sizeof(cpu_time_counters_t)));
	    memset(tsd->ending_cpu_counters,0,
		   (num_cpus * sizeof(cpu_time_counters_t)));
	    memset(tsd->delta_cpu_counters,0,
		   (num_cpus * sizeof(cpu_time_counters_t)));
	    SET_TEST_STATE(TEST_INIT);
	  } else {
	    /* lets see about cleaning-up some memory shall we? */
	    if (tsd->total_sys_counters) free(tsd->total_sys_counters);
	    if (tsd->starting_cpu_counters) free(tsd->starting_cpu_counters);
	    if (tsd->ending_cpu_counters) free(tsd->ending_cpu_counters);
	    if (tsd->delta_cpu_counters) free(tsd->delta_cpu_counters);
	    if (tsd->total_cpu_counters) free(tsd->total_cpu_counters);
	    
	    report_test_failure(test,
				"sys_stats",
				SYS_STATS_MALLOC_FAILED,
				"call to malloc failed");
	  }
	} else {
	  report_test_failure(test,
			      "sys_stats",
			      err,
			      "call to sys_cpu_util_init failed");
	}
	break;
      case TEST_INIT:
	if (test->debug) {
	  fprintf(test->where,"sys_stats: in INIT state\n");
	  fflush(test->where);
	}
	if (CHECK_REQ_STATE == TEST_IDLE) {
	  SET_TEST_STATE(TEST_IDLE);
	} else {
	  report_test_failure(test,
			      "sys_stats",
			      SYS_STATS_REQUESTED_STATE_INVALID,
			      "sys_stats found in TEST_INIT state");
	}
	break;
      case TEST_IDLE:
	if (test->debug) {
	  fprintf(test->where,"sys_stats: in IDLE state\n");
	  fflush(test->where);
	}
	/* check for state transition */
	if (CHECK_REQ_STATE == TEST_IDLE) {
	  g_usleep(1000000);
	} else if (CHECK_REQ_STATE == TEST_LOADED) {
	  SET_TEST_STATE(TEST_LOADED);
	} else if (CHECK_REQ_STATE == TEST_DEAD) {
	  SET_TEST_STATE(TEST_DEAD);
	} else {
	  report_test_failure(test,
			      "sys_stats",
			      SYS_STATS_REQUESTED_STATE_INVALID,
			      "sys_stats found in TEST_IDLE state");
	}
	break;
      case TEST_MEASURE:
	if (test->debug) {
	  fprintf(test->where,"sys_stats: in MEAS state\n");
	  fflush(test->where);
	}
	
	if (CHECK_REQ_STATE == TEST_MEASURE) {
	  g_usleep(1000000);
	} 
	else if (CHECK_REQ_STATE == TEST_LOADED) {
	  /* get_cpu_time_counters sets current timestamp */
	  get_cpu_time_counters(tsd->ending_cpu_counters,&(tsd->curr_time),test);
	  update_sys_stats(test);
	  SET_TEST_STATE(TEST_LOADED);
	} 
	else {
	  report_test_failure(test,
			      "sys_stats",
			      SYS_STATS_REQUESTED_STATE_INVALID,
			      "in TEST_MEASURED only TEST_LOADED is valid");
	}
	break;
      case TEST_LOADED:
	if (test->debug) {
	  fprintf(test->where,"sys_stats: in LOAD state\n");
	  fflush(test->where);
	}
	
	if (CHECK_REQ_STATE == TEST_LOADED) {
	  g_usleep(1000000);
	} else if (CHECK_REQ_STATE == TEST_MEASURE) {
	  /* transitioning to measure state from loaded state set
	     get_cpu_time_counters sets previous timestamp */
	  get_cpu_time_counters(tsd->starting_cpu_counters,&(tsd->prev_time),test);
	  SET_TEST_STATE(TEST_MEASURE);
	} else if (CHECK_REQ_STATE == TEST_IDLE) {
	  SET_TEST_STATE(TEST_IDLE);
	} else {
	  report_test_failure(test,
			      "sys_stats",
			      SYS_STATS_REQUESTED_STATE_INVALID,
			      "in TEST_LOADED state IDLE MEASURE valid");
	}
	break;
      default:
	report_test_failure(test,
			    "sys_stats",
			    SYS_STATS_STATE_CORRUPTED,
			    "sys_stats found in ILLEGAL state");
      } /* end of switch in while loop */
    } /* end of while for test */
    
    /* do we ever get here? seems that if we do, it would be spinning
       like crazy?!?  raj 2005-10-06 */
    while (GET_TEST_STATE != TEST_DEAD) {
      g_usleep(1000000);
      if (CHECK_REQ_STATE == TEST_DEAD) {
	SET_TEST_STATE(TEST_DEAD);
      }
    }
  }
}
