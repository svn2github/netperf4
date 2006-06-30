char netsysstats_none[]="\
@(#)(c) Copyright 2006, Hewlett-Packard Company, $Id: netsysstats_none.c 180 2006-05-24 20:02:03Z raj $";

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
#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#if STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif

#include <errno.h>

#include <kstat.h>
#include <sys/sysinfo.h>

#include "netperf.h"
#include "netsysstats.h"


/* these probably need to be placed in some test-specific area */
static kstat_ctl_t *kc = NULL;
static kid_t kcid = 0;

static void
print_unexpected_statistic_warning(test_t *test, char *who, char *what, char *why)
{
  if (why) {
    fprintf(test->where,
	    "WARNING! WARNING! WARNING! WARNING!\n");
    fprintf(test->where,
	    "%s found an unexpected %s statistic %.16s\n",
	    who,
	    why,
	    what);
  }
  else {
    fprintf(test->where,
	    "%s is ignoring statistic %.16s\n",
	    who,
	    what);
  }
}


int
sys_cpu_util_init(test_t *test) 
{

  netsysstat_data_t *tsd = GET_TEST_DATA(test);

  NETPERF_DEBUG_ENTRY(test->debug,test->where);
  tsd->num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  kc = kstat_open();

  if (kc == NULL) {
    fprintf(test->where,
	    "cpu_util_init: kstat_open: errno %d %s\n",
	    errno,
	    strerror(errno));
    fflush(test->where);
  }

  NETPERF_DEBUG_EXIT(test->debug, test->where);
  return NPE_SUCCESS;
}

static void
get_cpu_counters(test_t *test, int cpu_num, cpu_time_counters_t *counters)
{

  kstat_t *ksp;
  int found=0;
  kid_t nkcid;
  kstat_named_t *knp;
  int i;

  ksp = kstat_lookup(kc, "cpu", cpu_num, "sys");
  if ((ksp) && (ksp->ks_type == KSTAT_TYPE_NAMED)) {
    /* happiness and joy, keep going */
    nkcid = kstat_read(kc, ksp, NULL);
    if (nkcid != -1) {
      /* happiness and joy, keep going. we could consider adding a
	 "found < 3" to the end conditions, but then we wouldn't
	 search to the end and find that Sun added some nsec. we
	 probably want to see if they add an nsec. raj 2005-01-28 */
      for (i = ksp->ks_ndata, knp = ksp->ks_data;
	   i > 0;
	   knp++,i--) {
	/* we would be hosed if the same name could appear twice */
	if (!strcmp("cpu_nsec_idle",knp->name)) {
	  found++;
	  counters[cpu_num].idle = knp->value.ui64;
	}
	else if (!strcmp("cpu_nsec_user",knp->name)) {
	  found++;
	  counters[cpu_num].user = knp->value.ui64;
	}
	else if (!strcmp("cpu_nsec_kernel",knp->name)) {
	  found++;
	  counters[cpu_num].kernel = knp->value.ui64;
	}
	else if (strstr(knp->name,"nsec")) {
	  /* finding another nsec here means Sun have changed
	     something and we need to warn the user. raj 2005-01-28 */ 
	  print_unexpected_statistic_warning(test,
					     "get_cpu_counters",
					     knp->name,
					     "nsec");
	}
	else if (test->debug >=2) {

	  /* might want to tell people about what we are skipping.
	     however, only display other names debug >=2. raj
	     2005-01-28
	  */

	  print_unexpected_statistic_warning(test,
					     "get_cpu_counters",
					     knp->name,
					     NULL);
	}
      }
      if (3 == found) {
	/* happiness and joy */
	return;
      }
      else {
	fprintf(test->where,
		"get_cpu_counters could not find one or more of the expected counters!\n");
	fflush(test->where);
	/* we used to exit here but now I don't think we should */
      }
    }
    else {
      /* the kstat_read returned an error or the chain changed */
      fprintf(test->where,
	      "get_cpu_counters: kstat_read failed or chain id changed %d %s\n",
	      errno,
	      strerror(errno));
      fflush(test->where);
    }
  }
  else {
    /* the lookup failed or found the wrong type */
    fprintf(test->where,
	    "get_cpu_counters: kstat_lookup failed for module 'cpu' instance %d name 'sys' and KSTAT_TYPE_NAMED: errno %d %s\n",
	    cpu_num,
	    errno,
	    strerror(errno));
    fflush(test->where);
  }
}

static void
get_interrupt_counters(test_t *test, int cpu_num, cpu_time_counters_t *counters)
{
  kstat_t *ksp;
  int found=0;
  kid_t nkcid;
  kstat_named_t *knp;
  int i;

  ksp = kstat_lookup(kc, "cpu", cpu_num, "intrstat");

  counters[cpu_num].interrupt = 0;
  if ((ksp) && (ksp->ks_type == KSTAT_TYPE_NAMED)) {
    /* happiness and joy, keep going */
    nkcid = kstat_read(kc, ksp, NULL);
    if (nkcid != -1) {
      /* happiness and joy, keep going. we could consider adding a
	 "found < 15" to the end conditions, but then we wouldn't
	 search to the end and find that Sun added some "time." we
	 probably want to see if they add a "nsec." raj 2005-01-28 */
      for (i = ksp->ks_ndata, knp = ksp->ks_data;
	   i > 0;
	   knp++,i--) {
	if (strstr(knp->name,"time")) {
	  found++;
	  counters[cpu_num].interrupt += knp->value.ui64;
	}
	else if (test->debug >=2) {

	  /* might want to tell people about what we are skipping.
	     however, only display other names debug >=2. raj
	     2005-01-28
	  */

	  print_unexpected_statistic_warning(test,
					     "get_cpu_counters",
					     knp->name,
					     NULL);
	}
      }
      if (15 == found) {
	/* happiness and joy */
	return;
      }
      else {
	fprintf(test->where,
		"get_cpu_counters could not find one or more of the expected counters!\n");
	fflush(test->where);
      }
    }
    else {
      /* the kstat_read returned an error or the chain changed */
      fprintf(test->where,
	      "get_cpu_counters: kstat_read failed or chain id changed %d %s\n",
	      errno,
	      strerror(errno));
      fflush(test->where);
    }
  }
  else {
    /* the lookup failed or found the wrong type */
    fprintf(test->where,
	    "get_cpu_counters: kstat_lookup failed for module 'cpu' instance %d class 'intrstat' and KSTAT_TYPE_NAMED: errno %d %s\n",
	    cpu_num,
	    errno,
	    strerror(errno));
    fflush(test->where);
  }

}

void
get_cpu_time_counters(cpu_time_counters_t *res,
		      struct timeval *timestamp,
		      test_t *test)
{
  int                i;
  netsysstat_data_t *tsd = GET_TEST_DATA(test);
  int                num_cpus = tsd->num_cpus;
  int                cpu_util_kosher = 1;
  double             elapsed;

#define CALC_PERCENT 100
#define CALC_TENTH_PERCENT 1000
#define CALC_HUNDREDTH_PERCENT 10000
#define CALC_THOUSANDTH_PERCENT 100000
#define CALC_ACCURACY CALC_THOUSANDTH_PERCENT

  uint64_t fraction_idle;
  uint64_t fraction_user;
  uint64_t fraction_kernel;
  uint64_t fraction_interrupt;

  uint64_t interrupt_idle;
  uint64_t interrupt_user;
  uint64_t interrupt_kernel;

  uint64_t total_cpu_nsec;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  gettimeofday(timestamp,NULL);
  

  for (i = 0; i < num_cpus; i++) {
    elapsed = (double)timestamp->tv_sec + 
      ((double)timestamp->tv_usec / (double)1000000);
    res[i].calibrate = (uint64_t)elapsed * 1000000000; /* nanoseconds */

    /* this call will retrieve user/kernel/idle */
    get_cpu_counters(test, i, res);

    /* this call will retrieve interrupt */
    get_interrupt_counters(test, i, res);

    /* OK, here we have to do our big stinking pile of ass-u-me-ing so
       we can merge the user/kernel/idle counters with the interrupt
       time because Sun has them overlapping. from netperf2 we learn:

       this is now the fun part.  we have the nanoseconds _allegedly_
       spent in user, idle and kernel.  We also have nanoseconds spent
       servicing interrupts.  Sadly, in the developers' finite wisdom,
       the interrupt time accounting is in parallel (ie overlaps) with
       the other accounting. this means that time accounted in user,
       kernel or idle will also include time spent in interrupt.  for
       netperf's porpoises we do not _really_ care about that for user
       and kernel, but we certainly do care for idle.  the $64B
       question becomes - how to "correct" for this?

       we could just subtract interrupt time from idle.  that has the
       virtue of simplicity and also "punishes" Sun for doing
       something that seems to be so stupid.  however, we probably
       have to be "fair" even to the allegedly stupid so the other
       mechanism, suggested by a Sun engineer, is to subtract
       interrupt time from each of user, kernel and idle in proportion
       to their numbers.  then we sum the corrected user, kernel and
       idle along with the interrupt time and use that to calculate a
       new idle percentage and thus a CPU util percentage.

       that is what we will attempt to do here.  raj 2005-01-28 

       of course, we also have to wonder what we should do if there is
       more interrupt time than the sum of user, kernel and idle.
       that is a theoretical possibility I suppose, but for the
       time-being, one that we will blythly ignore, except perhaps for
       a quick check. raj 2005-01-31 
    */

    /* what differs here from netperf2 is we are trying this kludge on
       the raw counters rather than the delta because it is code
       "above" us that is taking the delta.  this may make some of our
       "wrap" assumptions a bit more brittle but that is life. */

    total_cpu_nsec = 
      res[i].idle +
      res[i].user +
      res[i].kernel;

    if (test->debug) {
      fprintf(test->where,
	      "%s pre-fixup total_cpu_nsec[%d] %"PRIu64"\n",
	      __func__,
	      i,
	      total_cpu_nsec);
    }

    /* not sure if we really need this check from netperf2 but just in
       case... raj 2006-06-29 */
    if (res[i].interrupt > total_cpu_nsec) {
      fprintf(test->where,
	      "WARNING! WARNING! WARNING! WARNING! WARNING! \n");
      fprintf(test->where,
	      "%s: more interrupt time for cpu %d than user/kern/idle combined!\n",
	      i,
	      __func__);
      fprintf(test->where,
	      "\tso CPU util cannot be reliably estimated\n");
      fflush(test->where);
      cpu_util_kosher = 0;
    }

    /* and now some fun with integer math.  i initially tried to
       promote things to long doubled but that didn't seem to result
       in happiness and joy. raj 2005-01-28 */

    fraction_idle = (res[i].idle * CALC_ACCURACY) / total_cpu_nsec;

    fraction_user = (res[i].user * CALC_ACCURACY) / total_cpu_nsec;

    fraction_kernel =  (res[i].kernel * CALC_ACCURACY) / total_cpu_nsec;

    /* ok, we have our fractions, now we want to take that fraction of
       the interrupt time and subtract that from the bucket. */

    interrupt_idle =  ((res[i].interrupt * fraction_idle) / 
		       CALC_ACCURACY);

    interrupt_user = ((res[i].interrupt * fraction_user) / 
		      CALC_ACCURACY);

    interrupt_kernel = ((res[i].interrupt * fraction_kernel) / 
			CALC_ACCURACY);

    if (test->debug) {
      fprintf(test->where,
	      "\tcpu[%d] fraction_idle %"PRIu64" interrupt_idle %"PRIu64"\n",
	      i,
	      fraction_idle,
	      interrupt_idle);
      fprintf(test->where,
	      "\tcpu[%d] fraction_user %"PRIu64" interrupt_user %"PRIu64"\n",
	      i,
	      fraction_user,
	      interrupt_user);
      fprintf(test->where,
	      "\tcpu[%d] fraction_kernel %"PRIu64" interrupt_kernel %"PRIu64"\n",
	      i,
	      fraction_kernel,
	      interrupt_kernel);
    }

    /* now we do our subtraction */
    res[i].idle = res[i].idle - interrupt_idle;

    res[i].user = res[i].user - interrupt_user;

    res[i].kernel = res[i].kernel - interrupt_kernel;

  }
  NETPERF_DEBUG_EXIT(test->debug, test->where);

}
