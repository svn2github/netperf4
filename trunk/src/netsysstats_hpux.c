char netsysstats_id[]="\
@(#)netsysstats_hpux.c (c) Copyright 2005, Hewlett-Packard Company, Version 4.0.0";

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

#include "config.h"

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

#include <sys/dk.h>
#include <sys/pstat.h>
#include "netperf.h"

#define GET_ERRNO errno

/* Macros for accessing fields in the global netperf structures. */
#define SET_TEST_STATE(state)             test->new_state = state
#define GET_TEST_STATE                    test->new_state
#define CHECK_REQ_STATE                   test->state_req
#define GET_TEST_DATA(test)               test->test_specific_data

/* global variables are used to keep copies of system statistics since
   only one instance of the test should be running on any system  */

/* still, one wonders if it might be more "consistent" to use the
   test-specific data section of the test structure... raj
   2005-06-10 */

typedef struct cpu_time_counters {
  uint64_t calibrate; /* the number by which all the rest should be
			 divided */
  uint64_t idle;      /* the number of time units in the idle state */
  uint64_t user;      /* the number of time units in the user state */
  uint64_t kernel;    /* the number of time units in the kernel state */
  uint64_t interrupt; /* the number of time units in the interrupt
			 state */
  uint64_t other;     /* the number of time units in other states
			 and/or any slop we have */
} cpu_time_counters_t;

uint64_t lib_iticksperclktick;

static cpu_time_counters_t  *starting_cpu_counters;
static cpu_time_counters_t  *ending_cpu_counters;
static cpu_time_counters_t  *delta_cpu_counters;
static cpu_time_counters_t  *total_cpu_counters;
static cpu_time_counters_t  *temp_cpu_counters;

enum {
  HPUX_SYS_STATS_PSTAT_GETPROCESSOR_FAILED = -5,
  HPUX_SYS_STATS_MALLOC_FAILED = -4,
  HPUX_SYS_STATS_GETDYNAMIC_FAILED = -3,
  HPUX_SYS_STATS_REQUESTED_STATE_INVALID = -2,
  HPUX_SYS_STATS_STATE_CORRUPTED = -1,
  HPUX_SYS_STATS_SUCCESS = 0
};

extern int   debug;
FILE *where = stderr;

int                 num_cpus;
int                 ticks;

struct timeval      prev_time;
struct timeval      curr_time;

struct timeval      total_elapsed_time;
struct timeval      delta_elapsed_time;

struct pst_processor *psp;

static void
get_cpu_time_counters(cpu_time_counters_t *res)
{
  /* get the idle sycle counter for each processor. now while on a
     64-bit kernel the ".psc_hi" and ".psc_lo" fields are 64 bits,
     only the bottom 32-bits are actually valid.  don't ask me why,
     that is just the way it is.  soo, we shift the psc_hi value by 32
     bits and then just sum-in the psc_lo value.  raj 2005/09/06 */ 

  if (pstat_getprocessor(psp, sizeof(*psp), num_cpus, 0) != -1) {
    int i;
    /* we use lib_iticksperclktick in our sanity checking. we
       ass-u-me it is the same value for each CPU - famous last
       words no doubt. raj 2005/09/06 */
    lib_iticksperclktick = psp[0].psp_iticksperclktick;
    for (i = 0; i < num_cpus; i++) {
      res[i].idle = (((uint64_t)psp[i].psp_idlecycles.psc_hi << 32) +
		     psp[i].psp_idlecycles.psc_lo);
      if(debug) {
	fprintf(where,
		"\tidle[%d] = 0x%"PRIx64" ",
		i,
		res[i].idle);
	fflush(where);
      }
#ifdef HPUX_1123
      res[i].user = (((uint64_t)psp[i].psp_usercycles.psc_hi << 32) +
		     psp[i].psp_usercycles.psc_lo);
      if(debug) {
	fprintf(where,
		"user[%d] = 0x%"PRIx64" ",
		i,
		res[i].user);
	fflush(where);
      }
      res[i].kernel = (((uint64_t)psp[i].psp_systemcycles.psc_hi << 32) +
		       psp[i].psp_systemcycles.psc_lo);
      if(debug) {
	fprintf(where,
		"kern[%d] = 0x%"PRIx64" ",
		i,
		res[i].kernel);
	fflush(where);
      }
      res[i].interrupt = (((uint64_t)psp[i].psp_interruptcycles.psc_hi << 32) +
			  psp[i].psp_interruptcycles.psc_lo);
      if(debug) {
	fprintf(where,
		"intr[%d] = 0x%"PRIx64"\n",
		i,
		res[i].interrupt);
	fflush(where);
      }
#else
      res[i].user = 0;
      res[i].kernel = 0;
      res[i].interrupt = 0;
#endif
    }
  }
}

static void
update_elapsed_times(struct timeval *curr, struct timeval *prev)
{
  delta_elapsed_time.tv_usec = curr->tv_usec - prev->tv_usec;
  delta_elapsed_time.tv_sec  = curr->tv_sec  - prev->tv_sec;

  if (curr->tv_usec < prev->tv_usec) {
    delta_elapsed_time.tv_usec += 1000000;
    delta_elapsed_time.tv_sec--;
  }

  if (delta_elapsed_time.tv_usec >= 1000000) {
    delta_elapsed_time.tv_usec -= 1000000;
    delta_elapsed_time.tv_sec++;
  }

  total_elapsed_time.tv_sec += delta_elapsed_time.tv_sec;
  total_elapsed_time.tv_usec += delta_elapsed_time.tv_usec;

  if (total_elapsed_time.tv_usec >= 1000000) {
    total_elapsed_time.tv_usec -= 1000000;
    total_elapsed_time.tv_sec++;
  }
}

/* NOTE, update_elapsed_times() MUST be called before this routine */
static void
update_sys_stats()
{
  int        i;
  double elapsed;
  uint64_t total;

  elapsed = (double)delta_elapsed_time.tv_sec +
    ((double)delta_elapsed_time.tv_usec / (double)1000000);

  for (i = 0; i < num_cpus; i++) {
    total = 0;

    delta_cpu_counters[i].calibrate = 
      (uint64_t) (elapsed * ticks * (double) lib_iticksperclktick);

    delta_cpu_counters[i].idle = 
      ending_cpu_counters[i].idle - starting_cpu_counters[i].idle;
    if (ending_cpu_counters[i].idle < starting_cpu_counters[i].idle) {
      /* if these are unsigned 64 bit integers, shouldn't that be
	 ULONG_LONG_MAX or something?  raj 2005-10-06 */
      delta_cpu_counters[i].idle += LONG_LONG_MAX;
    }

    total += delta_cpu_counters[i].idle;

    delta_cpu_counters[i].user = 
      ending_cpu_counters[i].user - starting_cpu_counters[i].user;
    if (ending_cpu_counters[i].user < starting_cpu_counters[i].user) {
      /* if these are unsigned 64 bit integers, shouldn't that be
	 ULONG_LONG_MAX or something?  raj 2005-10-06 */
      delta_cpu_counters[i].user += LONG_LONG_MAX;
    }

    total += delta_cpu_counters[i].user;

    delta_cpu_counters[i].kernel = 
      ending_cpu_counters[i].kernel - starting_cpu_counters[i].kernel;
    if (ending_cpu_counters[i].kernel < starting_cpu_counters[i].kernel) {
      /* if these are unsigned 64 bit integers, shouldn't that be
	 ULONG_LONG_MAX or something?  raj 2005-10-06 */
      delta_cpu_counters[i].kernel += LONG_LONG_MAX;
    }

    total += delta_cpu_counters[i].kernel;

    delta_cpu_counters[i].interrupt = 
      ending_cpu_counters[i].interrupt - starting_cpu_counters[i].interrupt;
    if (ending_cpu_counters[i].interrupt < starting_cpu_counters[i].interrupt) {
      /* if these are unsigned 64 bit integers, shouldn't that be
	 ULONG_LONG_MAX or something?  raj 2005-10-06 */
      delta_cpu_counters[i].interrupt += LONG_LONG_MAX;
    }

    total += delta_cpu_counters[i].interrupt;

    delta_cpu_counters[i].other = delta_cpu_counters[i].calibrate - total;
    if (total > delta_cpu_counters[i].calibrate) {
      /* hmm, what is the right thing to do here? for now simply
	 zero-out other. in the future consider emitting a
	 warning. raj 2005-10-06 */
      delta_cpu_counters[i].other = 0;
    }
    
    total_cpu_counters[i].calibrate += delta_cpu_counters[i].calibrate;
    total_cpu_counters[i].idle += delta_cpu_counters[i].idle;
    total_cpu_counters[i].user += delta_cpu_counters[i].user;
    total_cpu_counters[i].kernel += delta_cpu_counters[i].kernel;
    total_cpu_counters[i].interrupt += delta_cpu_counters[i].interrupt;
    total_cpu_counters[i].other += delta_cpu_counters[i].other;
  }

}
  
static void
report_test_failure(test, function, err_code, err_string)
  test_t *test;
  char   *function;
  int     err_code;
  char   *err_string;
{
  test->err_no    = GET_ERRNO;
  if (debug) {
    fprintf(where,"%s: called report_test_failure:\n",function);
    fprintf(where,"\nreporting  %s  errno = %d\n",err_string,test->err_no);
    fflush(where);
  }
  test->err_rc    = err_code;
  test->err_fn    = function;
  test->err_str   = err_string;
  test->new_state = TEST_ERROR;
}

static void
clear_cpu_time_counters(cpu_time_counters_t *counters) {
  /* lets be paranoid */
  if (NULL != counters) {
    counters->calibrate = 0;
    counters->idle = 0;
    counters->user = 0;
    counters->kernel = 0;
    counters->interrupt = 0;
    counters->other = 0;
  }
  else {
    /* probably aught to report something here */
  }
}

int
hpux_sys_stats_clear_stats(test_t *test)
{
  total_elapsed_time.tv_usec = 0;
  total_elapsed_time.tv_sec  = 0;
  delta_elapsed_time.tv_usec = 0;
  delta_elapsed_time.tv_sec  = 0;

  clear_cpu_time_counters(starting_cpu_counters);
  clear_cpu_time_counters(ending_cpu_counters);
  clear_cpu_time_counters(total_cpu_counters);
  clear_cpu_time_counters(delta_cpu_counters);

  gettimeofday(&prev_time, NULL);
  curr_time  = prev_time;
  return(NPE_SUCCESS);
}

static void
set_repeating_value(char *buffer, size_t buflen, int count, char *format, void *value) {
  int i, ret;
  
  for(i = 0; i < count; i++) {
    ret = snprintf(buffer,buflen,format,value);
    if (ret > 0) {
      buflen -= ret;
      buffer += ret;
    }
  }

}
static xmlNodePtr
sys_stats_get_stats(test_t *test)
{
  xmlNodePtr stats = NULL;
  xmlAttrPtr ap    = NULL;
  char       *value= NULL;
  char       *temp = NULL;
  int        val_len,remaining,ret,i;
  
  if (debug) {
    fprintf(where,"sys_stats_get_stats: entering for %s test %s\n",
            test->id, test->test_name);
    fflush(where);
  }

  /* allocate the scratch buffer to hold the per-CPU statistics. for
     now ass-u-me that each can take up to 32 bytes, which was
     previously the size of value before going per-CPU.  this is
     likely a bit on the conservative side. raj 2005-10-06 */
  val_len = num_cpus * 32;
  value = malloc(val_len);

  if ((NULL != value) &&
      (stats = xmlNewNode(NULL,(xmlChar *)"sys_stats")) != NULL) {
    /* set the properites of the sys_stats message -
       the tid and time stamps/values and counter values  sgb 2004-10-27 */

    ap = xmlSetProp(stats,(xmlChar *)"tid",test->id);
    if (GET_TEST_STATE == TEST_MEASURE) {
      gettimeofday(&curr_time, NULL);
      if (ap != NULL) {
        sprintf(value,"%#ld",curr_time.tv_sec);
        ap = xmlSetProp(stats,(xmlChar *)"time_sec",(xmlChar *)value);
        if (debug) {
          fprintf(where,"time_sec=%s\n",value);
          fflush(where);
        }
      }
      if (ap != NULL) {
        sprintf(value,"%#ld",curr_time.tv_usec);
        ap = xmlSetProp(stats,(xmlChar *)"time_usec",(xmlChar *)value);
        if (debug) {
          fprintf(where,"time_usec=%s\n",value);
          fflush(where);
        }
      }
    } else {
      if (ap != NULL) {
        sprintf(value,"%#ld",total_elapsed_time.tv_sec);
        ap = xmlSetProp(stats,(xmlChar *)"elapsed_sec",(xmlChar *)value);
        if (debug) {
          fprintf(where,"elapsed_sec=%s\n",value);
          fflush(where);
        }
      }
      if (ap != NULL) {
        sprintf(value,"%#ld",total_elapsed_time.tv_usec);
        ap = xmlSetProp(stats,(xmlChar *)"elapsed_usec",(xmlChar *)value);
        if (debug) {
          fprintf(where,"elapsed_usec=%s\n",value);
          fflush(where);
        }
      }
    }
    if (ap != NULL) {
      sprintf(value,"%#ld",num_cpus);
      ap = xmlSetProp(stats,(xmlChar *)"number_cpus",(xmlChar *)value);
      if (debug) {
        fprintf(where,"number_cpus=%s\n",value);
        fflush(where);
      }
    }
    if (ap != NULL) {
      temp = value;
      remaining = val_len;
      for (i = 0; i < num_cpus; i++) {
	ret = snprintf(temp,
		       remaining,
		       "%#ld ",
		       total_cpu_counters[i].calibrate);
	if (ret > 0) {
	  remaining -= ret;
	  temp += ret;
	}
      }
      ap = xmlSetProp(stats,(xmlChar *)"calibration",(xmlChar *)value);
      if (debug) {
        fprintf(where,"calibration=%s\n",value);
        fflush(where);
      }
    }
    if (ap != NULL) {
      temp = value;
      remaining = val_len;
      for (i = 0; i < num_cpus; i++) {
	ret = snprintf(temp,
		       remaining,
		       "%#ld ",
		       total_cpu_counters[i].idle);
	if (ret > 0) {
	  remaining -= ret;
	  temp += ret;
	}
      }
      ap = xmlSetProp(stats,(xmlChar *)"idle_count",(xmlChar *)value);
      if (debug) {
        fprintf(where,"idle_count=%s\n",value);
        fflush(where);
      }
    }
    if (ap != NULL) {
      temp = value;
      remaining = val_len;
      for (i = 0; i < num_cpus; i++) {
	ret = snprintf(temp,
		       remaining,
		       "%#ld ",
		       total_cpu_counters[i].user);
	if (ret > 0) {
	  remaining -= ret;
	  temp += ret;
	}
      }
      ap = xmlSetProp(stats,(xmlChar *)"user_count",(xmlChar *)value);
      if (debug) {
        fprintf(where,"user_count=%s\n",value);
        fflush(where);
      }
    }
    if (ap != NULL) {
      temp = value;
      remaining = val_len;
      for (i = 0; i < num_cpus; i++) {
	ret = snprintf(temp,
		       remaining,
		       "%#ld ",
		       total_cpu_counters[i].kernel);
	if (ret > 0) {
	  remaining -= ret;
	  temp += ret;
	}
      }
      ap = xmlSetProp(stats,(xmlChar *)"sys_count",(xmlChar *)value);
      if (debug) {
        fprintf(where,"sys_count=%s\n",value);
        fflush(where);
      }
    }
    if (ap != NULL) {
      temp = value;
      remaining = val_len;
      for (i = 0; i < num_cpus; i++) {
	ret = snprintf(temp,
		       remaining,
		       "%#ld ",
		       total_cpu_counters[i].interrupt);
	if (ret > 0) {
	  remaining -= ret;
	  temp += ret;
	}
      }
      ap = xmlSetProp(stats,(xmlChar *)"int_count",(xmlChar *)value);
      if (debug) {
        fprintf(where,"int_count=%s\n",value);
        fflush(where);
      }
    }
    if (ap == NULL) {
      xmlFreeNode(stats);
      stats == NULL;
    }
  }
  if (debug) {
    fprintf(where,"sys_stats_get_stats: exiting for %s test %s\n",
            test->id, test->test_name);
    fflush(where);
  }
  if (NULL != value) free(value);
  return(stats);
}

xmlNodePtr
hpux_sys_stats_get_stats(test_t *test)
{
  return( sys_stats_get_stats(test));
}

void
hpux_sys_stats_decode_stats(xmlNodePtr stats, test_t *test)
{
}

void
hpux_sys_stats(test_t *test)
{
  int i,n;
  struct pst_dynamic    psd;
  
  while ((GET_TEST_STATE != TEST_ERROR) &&
         (GET_TEST_STATE != TEST_DEAD)) {
    switch (GET_TEST_STATE) {
    case TEST_PREINIT:
      if (pstat_getdynamic(&psd, sizeof(psd), (size_t)1, 0) != -1) {
        ticks     = sysconf(_SC_CLK_TCK);
        num_cpus  = psd.psd_proc_cnt;
        if (debug) {
          fprintf(where,"hpux_sys_stats: num_cpus = %d\n\n",num_cpus);
          fflush(where);
        }
        psp       = (struct pst_processor *)malloc(num_cpus * sizeof(*psp));
        if (debug) {
          fprintf(where,"hpux_sys_stats: allocated pst_processor\n");
          fflush(where);
        }

        if (debug) {
          fprintf(where,"hpux_sys_stats: allocating counters\n");
          fflush(where);
        }
	starting_cpu_counters = 
	  (cpu_time_counters_t *)malloc(num_cpus * 
					sizeof(starting_cpu_counters));
	ending_cpu_counters = 
	  (cpu_time_counters_t *)malloc(num_cpus * 
					sizeof(ending_cpu_counters));
	delta_cpu_counters = 
	  (cpu_time_counters_t *)malloc(num_cpus * 
					sizeof(delta_cpu_counters));
	total_cpu_counters = 
	  (cpu_time_counters_t *)malloc(num_cpus * 
					sizeof(total_cpu_counters));

	if ((NULL != psp) && 
	    (NULL != starting_cpu_counters) &&
	    (NULL != ending_cpu_counters) &&
	    (NULL != delta_cpu_counters) &&
	    (NULL != total_cpu_counters)) {
          SET_TEST_STATE(TEST_INIT);
        } else {
          report_test_failure(test,
                              "hpux_sys_stats",
                              HPUX_SYS_STATS_MALLOC_FAILED,
                              "call to malloc failed");
        }
      } else {
        report_test_failure(test,
                            "hpux_sys_stats",
                            HPUX_SYS_STATS_GETDYNAMIC_FAILED,
                            "call to pstat_getdynamic failed");
      }
      break;
    case TEST_INIT:
      if (CHECK_REQ_STATE == TEST_IDLE) {
        if (debug) {
          fprintf(where,"hpux_sys_stats: in INIT state\n");
          fflush(where);
        }
        SET_TEST_STATE(TEST_IDLE);
      } else {
        report_test_failure(test,
                            "hpux_sys_stats",
                            HPUX_SYS_STATS_REQUESTED_STATE_INVALID,
                            "hpux_sys_stats found in TEST_INIT state");
      }
      break;
    case TEST_IDLE:
      /* check for state transition */
      if (CHECK_REQ_STATE == TEST_IDLE) {
        sleep(1);
      } else if (CHECK_REQ_STATE == TEST_LOADED) {
        SET_TEST_STATE(TEST_LOADED);
      } else if (CHECK_REQ_STATE == TEST_DEAD) {
        SET_TEST_STATE(TEST_DEAD);
      } else {
        report_test_failure(test,
                            "hpux_sys_stats",
                            HPUX_SYS_STATS_REQUESTED_STATE_INVALID,
                            "hpux_sys_stats found in TEST_IDLE state");
      }
      break;
    case TEST_MEASURE:
      if (CHECK_REQ_STATE == TEST_MEASURE) {
        sleep(1);
      } else if (CHECK_REQ_STATE == TEST_LOADED) {
	get_cpu_time_counters(ending_cpu_counters);
	gettimeofday(&curr_time, NULL);
	update_elapsed_times(&curr_time,&prev_time);
	update_sys_stats();
	SET_TEST_STATE(TEST_LOADED);
      } else {
        report_test_failure(test,
                            "hpux_sys_stats",
                            HPUX_SYS_STATS_REQUESTED_STATE_INVALID,
                            "in TEST_MEASURED only TEST_LOADED is valid");
      }
      break;
    case TEST_LOADED:
      if (CHECK_REQ_STATE == TEST_LOADED) {
        sleep(1);
      } else if (CHECK_REQ_STATE == TEST_MEASURE) {
	get_cpu_time_counters(starting_cpu_counters);
	/* transitioning to measure state from loaded state set
	   previous timestamp */
	gettimeofday(&prev_time, NULL);
	SET_TEST_STATE(TEST_MEASURE);
      } else if (CHECK_REQ_STATE == TEST_IDLE) {
        SET_TEST_STATE(TEST_IDLE);
      } else {
        report_test_failure(test,
                            "hpux_sys_stats",
                            HPUX_SYS_STATS_REQUESTED_STATE_INVALID,
                            "in TEST_LOADED state IDLE MEASURE valid");
      }
      break;
    default:
      report_test_failure(test,
                          "hpux_sys_stats",
                          HPUX_SYS_STATS_STATE_CORRUPTED,
                          "hpux_sys_stats found in ILLEGAL state");
    } /* end of switch in while loop */
  } /* end of while for test */

  /* do we ever get here? seems that if we do, it would be spinning
     like crazy?!?  raj 2005-10-06 */
  while (GET_TEST_STATE != TEST_DEAD) {
    if (CHECK_REQ_STATE == TEST_DEAD) {
      free(psp);
      SET_TEST_STATE(TEST_DEAD);
    }
  }
}
