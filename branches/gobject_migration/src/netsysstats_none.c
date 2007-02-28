char netsysstats_none[]="\
@(#)(c) Copyright 2006, Hewlett-Packard Company, $Id$";

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

#include "netperf.h"
#include "netsysstats.h"

int
sys_cpu_util_init(test_t *test) 
{

  netsysstat_data_t *tsd = GET_TEST_DATA(test);

  NETPERF_DEBUG_ENTRY(test->debug,test->where);
  tsd->num_cpus = 1;
  NETPERF_DEBUG_EXIT(test->debug, test->where);
  return NPE_SUCCESS;
}

/* slightly kludgy to get the CPU util to come-out as 100% */
static uint64_t user,kernel,other,interrupt,idle = 0;

void
get_cpu_time_counters(cpu_time_counters_t *res,
		      struct timeval *timestamp,
		      test_t *test)
{
  int i;
  netsysstat_data_t *tsd = GET_TEST_DATA(test);


  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  gettimeofday(timestamp,NULL);

  /* this should result in the CPU util being forever reported as
     100% */

  user += 10000;
  kernel += 10000;
  interrupt += 10000;
  idle += 1;

  for (i = 0; i < tsd->num_cpus; i++) {
    res[i].user = user;
    res[i].kernel = kernel;
    res[i].interrupt = interrupt;
    res[i].idle = idle;
    res[i].calibrate = res[i].user +
      res[i].kernel +
      res[i].interrupt +
      res[i].idle;
    res[i].other = 0;
  }

  NETPERF_DEBUG_EXIT(test->debug, test->where);

}
