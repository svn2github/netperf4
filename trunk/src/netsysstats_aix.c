char netsysstats_specific_id[]="\
@(#)(c) Copyright 2005, Hewlett-Packard Company, $Id$";

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

#include <libperfstat.h>

#include "netperf.h"
#include "netlib.h"
#include "netsysstats.h"

/* data is kept is malloced data structures which are referenced from
   the test specific data structure.  a pointer to platform specific data
   is passed for holding data needed for each platform.  255-10-18 sgb */

enum {
  HPUX_SYS_STATS_PSTAT_GETPROCESSOR_FAILED = -5,
  HPUX_SYS_STATS_MALLOC_FAILED = -4,
  HPUX_SYS_STATS_GETDYNAMIC_FAILED = -3,
  HPUX_SYS_STATS_REQUESTED_STATE_INVALID = -2,
  HPUX_SYS_STATS_STATE_CORRUPTED = -1,
  HPUX_SYS_STATS_SUCCESS = 0
};


int
sys_cpu_util_init(test_t *test)
{
  int num_cpus;
  perfstat_cpu_total_t *perfstat_total_buffer;

  int ret;
  int err = HPUX_SYS_STATS_SUCCESS;
  netsysstat_data_t *tsd = GET_TEST_DATA(test);

  /* one of these days I really aught to check the return value */
  perfstat_total_buffer = (perfstat_cpu_total_t *)
    malloc(sizeof(perfstat_cpu_total_t));

  /* while they seem to have kept the parms similar between
     perfstat_cpu and perfstat_cpu_total, it seems that
     perfstat_cpu_total never wants a "real" name parm given to it. I
     suppose that if I were paranoid, I'd call perfstat_cpu_total once
     with the first two parms NULL to get the number of totals, but
     for now we just ass-u-me it will be one :) raj 2006-04-07 */

  ret = perfstat_cpu_total(NULL,
			   perfstat_total_buffer,
			   sizeof(perfstat_cpu_total_t),
			   1);

  tsd->num_cpus = perfstat_total_buffer->ncpus;

  tsd->method = "AIX_PERFSTAT";
  /* save the pointer to the perfstat_total_buffer - from it we will
     be grabbing the processor frequency, which we use for a
     calibration-free CPU utilization measurement. */
  tsd->psd    = perfstat_total_buffer;

  return(err);
}

/* this file is the one we use if we aren't on AIX 5.3, which means we
   need to limit ourselves to using the idle, sys and user fields and
   cannot use the added-in-5.3 pidle, psys and puser fields.  those
   are used in the netsysstats_aix53.c file */

void
get_cpu_time_counters(cpu_time_counters_t *res,
                      struct timeval *timestamp,
		      test_t *test)
{

  netsysstat_data_t *tsd = GET_TEST_DATA(test);
  perfstat_cpu_total_t *psp      = tsd->psd;
  perfstat_cpu_t       *per_cpu_data;
  perfstat_id_t        name;
  int                   num_cpus = tsd->num_cpus;
  int                   i;
  double                elapsed;

  strcpy(name.name,"");

  per_cpu_data = (perfstat_cpu_t *)malloc(num_cpus *
					  sizeof(perfstat_cpu_t));

  if (perfstat_cpu(&name,
		   per_cpu_data,
		   sizeof(perfstat_cpu_t),
		   num_cpus) == num_cpus) {
    gettimeofday(timestamp, NULL);
    /* "elapsed" isn't "really" elapsed, but it works for what we want */
    elapsed = (double)timestamp->tv_sec + 
      ((double)timestamp->tv_usec / (double)1000000);

    for (i = 0; i < num_cpus; i++) {
      /* our "calibration" value will be our "elapsed" time multiplied
	 by the value of _SC_CLK_TCK. */
      res[i].calibrate = (uint64_t)(elapsed * 
				    (double) sysconf(_SC_CLK_TCK));
      res[i].idle      = per_cpu_data[i].idle;
#ifndef EXTRA_COUNTERS_MISSING
      res[i].user      = per_cpu_data[i].user;
      res[i].kernel    = per_cpu_data[i].sys;
      res[i].interrupt = 0;

      res[i].other     = res[i].calibrate;
      res[i].other    -= res[i].idle;
      res[i].other    -= res[i].user;
      res[i].other    -= res[i].kernel;
      res[i].other    -= res[i].interrupt;
#endif
      
      if(test->debug) {
	fprintf(test->where, "\tcalibrate[%d] = %"PRIx64, i, res[i].calibrate);
	fprintf(test->where, "\tidle[%d] = %"PRIx64, i, res[i].idle);
#ifndef EXTRA_COUNTERS_MISSING
	fprintf(test->where, "\tuser[%d] = %"PRIx64, i, res[i].user);
	fprintf(test->where, "\tkern[%d] = %"PRIx64, i, res[i].kernel);
	fprintf(test->where, "\tintr[%d] = %"PRIx64, i, res[i].interrupt);
	fprintf(test->where, "\tothr[%d] = %"PRIx64, i, res[i].other);
#endif
        fprintf(test->where, "\n");
	fflush(test->where);
      }
    }
    if (test->debug) {
      fprintf(test->where, "\tseconds=%d\tusec=%d\telapsed=%f\n",
              timestamp->tv_sec,timestamp->tv_usec,elapsed);
      fflush(test->where);
    }
  }
}

