char netsysstats_specific_id[]="\
@(#)netsysstats_hpux.c (c) Copyright 2005, Hewlett-Packard Company, $Id$";

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

#include <sys/dk.h>
#include <sys/pstat.h>

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
  struct pst_processor *psp;
  struct pst_dynamic    psd;
  int num_cpus;
  int err = HPUX_SYS_STATS_SUCCESS;
  netsysstat_data_t *tsd = GET_TEST_DATA(test);

  if (pstat_getdynamic(&psd, sizeof(psd), (size_t)1, 0) != -1) {
    num_cpus  = psd.psd_proc_cnt;
    if (test->debug) {
      fprintf(test->where,"sys_cpu_util_init: num_cpus = %d\n\n",num_cpus);
      fflush(test->where);
    }
    psp   = (struct pst_processor *)malloc(num_cpus * sizeof(*psp));
    if (test->debug) {
      fprintf(test->where,"sys_cpu_util_init: allocated pst_processor\n");
      fflush(test->where);
    }
    tsd->num_cpus = num_cpus;
    tsd->psd      = psp;
    tsd->method   = "HPUX_PSTAT_NEW";
  } else {
    err = HPUX_SYS_STATS_GETDYNAMIC_FAILED;
  }
  return(err);
}

void
get_cpu_time_counters(cpu_time_counters_t *res,
                      struct timeval *time,
		      test_t *test)
{

  netsysstat_data_t *tsd = GET_TEST_DATA(test);
  struct pst_processor *psp      = tsd->psd;
  int                   num_cpus = tsd->num_cpus;
  int                   i;
  double                ticks;
  double                iticksperclktick;
  double                elapsed;

  /* get the idle cycle counter for each processor. now while on a
     64-bit kernel the ".psc_hi" and ".psc_lo" fields are 64 bits,
     only the bottom 32-bits are actually valid.  don't ask me why,
     that is just the way it is.  soo, we shift the psc_hi value by 32
     bits and then just sum-in the psc_lo value.  raj 2005/09/06 */ 

  if (pstat_getprocessor(psp, sizeof(*psp), num_cpus, 0) != -1) {
    gettimeofday(time, NULL);
    elapsed = (double)time->tv_sec + ((double)time->tv_usec / (double)1000000);
    ticks   = sysconf(_SC_CLK_TCK);
    for (i = 0; i < num_cpus; i++) {
      /* we use lib_iticksperclktick in our sanity checking.  raj 2005/09/06 */
      iticksperclktick = (double)psp[i].psp_iticksperclktick;
      res[i].calibrate = (uint64_t)(elapsed * ticks * iticksperclktick);
      res[i].idle      = (((uint64_t)psp[i].psp_idlecycles.psc_hi << 32) +
		          psp[i].psp_idlecycles.psc_lo);
#ifndef EXTRA_COUNTERS_MISSING
      res[i].user      = (((uint64_t)psp[i].psp_usercycles.psc_hi << 32) +
		          psp[i].psp_usercycles.psc_lo);
      res[i].kernel    = (((uint64_t)psp[i].psp_systemcycles.psc_hi << 32) +
		          psp[i].psp_systemcycles.psc_lo);
      res[i].interrupt = (((uint64_t)psp[i].psp_interruptcycles.psc_hi << 32) +
			  psp[i].psp_interruptcycles.psc_lo);
      res[i].other     = res[i].calibrate;
      res[i].other    -= res[i].idle;
      res[i].other    -= res[i].user;
      res[i].other    -= res[i].kernel;
      res[i].other    -= res[i].interrupt;
#endif
      
      if(test->debug) {
	fprintf(test->where, "\tidle[%d] = %#llx", i, res[i].idle);
#ifndef EXTRA_COUNTERS_MISSING
	fprintf(test->where, "\tuser[%d] = %#llx", i, res[i].user);
	fprintf(test->where, "\tkern[%d] = %#llx", i, res[i].kernel);
	fprintf(test->where, "\tintr[%d] = %#llx", i, res[i].interrupt);
	fprintf(test->where, "\tothr[%d] = %#llx", i, res[i].other);
#endif
        fprintf(test->where, "\n");
	fflush(test->where);
      }
    }
    if (test->debug) {
      fprintf(test->where, "\tseconds=%d\tusec=%d\telapsed=%f\n",
              time->tv_sec,time->tv_usec,elapsed);
      fflush(test->where);
    }
  }
}

