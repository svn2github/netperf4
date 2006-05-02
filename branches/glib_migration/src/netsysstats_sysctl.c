char netsysstats_sysctl[]="\
@(#)Copyright 2005, Hewlett-Packard Company, $Id$";

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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif 

#include <sys/sysctl.h>

#include <errno.h>

#include "netperf.h"
#include "netsysstats.h"

/* this has been liberally cut and pasted from <sys/resource.h> on
   FreeBSD. in general, this would be a bad idea, but I don't want to
   have to do a _KERNEL define to get these and that is what
   sys/resource.h seems to want. raj 2002-03-03 */
#define CP_USER         0
#define CP_NICE         1
#define CP_SYS          2
#define CP_INTR         3
#define CP_IDLE         4
#define CPUSTATES       5

enum {
  SYSCTL_SYS_STATS_PROCSTAT_OPEN_FAILED = -5,
  SYSCTL_SYS_STATS_MALLOC_FAILED = -4,
  SYSCTL_SYS_STATS_SYSCONF_FAILED = -3,
  SYSCTL_SYS_STATS_REQUESTED_STATE_INVALID = -2,
  SYSCTL_SYS_STATS_STATE_CORRUPTED = -1,
  SYSCTL_SYS_STATS_SUCCESS = 0
};


int                 ticks;

int
sys_cpu_util_init(test_t *test) 
{

  netsysstat_data_t *tsd = GET_TEST_DATA(test);

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  if ((tsd->num_cpus = sysconf(_SC_NPROCESSORS_ONLN)) == -1) {
    return(SYSCTL_SYS_STATS_SYSCONF_FAILED);
  }

  return SYSCTL_SYS_STATS_SUCCESS;
}

  /* get the cpu ticks from /proc/stat for each CPU.  There are four
     potentially interesting values on each cpu line - idle, user,
     nice and kernel.  The netperf "model" is to track idle, user,
     kernel and interrupt, with a bucket for "other." so the question
     is should the nice time be folded into user, or should it be
     stuck in other?  for now, we will stick it into user since it is
     user-space time (?) raj 2005-10-07 */

void
get_cpu_time_counters(cpu_time_counters_t *res,
		      struct timeval *timestamp,
		      test_t *test)
{

  long cpu_time[CPUSTATES];
  size_t cpu_time_len = CPUSTATES * sizeof (cpu_time[0]);

  int i,records;
  uint64_t nicetime;
  netsysstat_data_t *tsd = GET_TEST_DATA(test);
  double elapsed;                   /* well, it isn't really "elapsed" */

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  gettimeofday(timestamp,NULL);
  elapsed = (double)timestamp->tv_sec + 
    ((double)timestamp->tv_usec / (double)1000000);
  if (test->debug) {
    fprintf(test->where,
	    "%s res %p timeptr %p test %p tsd %p\n",
	    __func__,
	    res,
	    timestamp,
	    test,
	    tsd);
    fflush(test->where);
  }

  if (sysctlbyname("kern.cp_time", cpu_time, &cpu_time_len, NULL, 0) == -1) {
      fprintf (stderr, "Cannot get CPU time!\n");
      exit (1);
  }
  
  /* the sysctlbyname gives us overall CPU utilization. rather than
     pretend the system is a UP, which would be inaccurate, we will
     ass-u-me that each CPU was "average" this means we must multiply
     the calibration by the number of CPUs in the system. raj
     2006-05-02 */
  for (i = 0; i < tsd->num_cpus; i++) {

    res[i].calibrate = (uint64_t)(elapsed * (double)sysconf(_SC_CLK_TCK));
    res[i].calibrate *= tsd->num_cpus;
    res[i].user      = cpu_time[CP_USER] + cpu_time[CP_NICE];
    res[i].kernel    = cpu_time[CP_SYS];
    res[i].interrupt = cpu_time[CP_INTR];
    res[i].idle      = cpu_time[CP_IDLE];

    res[i].other     = res[i].calibrate;
    res[i].other    -= res[i].idle;
    res[i].other    -= res[i].user;
    res[i].other    -= res[i].kernel;
    res[i].other    -= res[i].interrupt;

    if (test->debug) {
      fprintf(test->where,
	      "\tcalibrate[%d] = 0x%"PRIx64" ",
	      i,
	      res[i].calibrate);
      fprintf(test->where,
	      "\tidle[%d] = 0x%"PRIx64" ",
	      i,
	      res[i].idle);
      fprintf(test->where,
	      "user[%d] = 0x%"PRIx64" ",
	      i,
	      res[i].user);
      fprintf(test->where,
	      "kern[%d] = 0x%"PRIx64" ",
	      i,
	      res[i].kernel);
      fflush(test->where);
      fprintf(test->where,
	      "intr[%d] = 0x%"PRIx64"\n",
	      i,
	      res[i].interrupt);
    }
  }
}
