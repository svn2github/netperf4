char netsysstats_linux[]="\
@(#)netsysstats_linux.c (c) Copyright 2005, Hewlett-Packard Company, $Id$";

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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "netperf.h"
#include "netsysstats.h"

/* The max. length of one line of /proc/stat cpu output */
#define CPU_LINE_LENGTH ((8 * sizeof (long) / 3 + 1) * 4 + 8)
#define PROC_STAT_FILE_NAME "/proc/stat"
#define N_CPU_LINES(nr) (nr == 1 ? 1 : 1 + nr)

static int proc_stat_fd = -1;
static char* proc_stat_buf = NULL;
static int proc_stat_buflen = 0;

enum {
  LINUX_SYS_STATS_PROCSTAT_OPEN_FAILED = -5,
  LINUX_SYS_STATS_MALLOC_FAILED = -4,
  LINUX_SYS_STATS_SYSCONF_FAILED = -3,
  LINUX_SYS_STATS_REQUESTED_STATE_INVALID = -2,
  LINUX_SYS_STATS_STATE_CORRUPTED = -1,
  LINUX_SYS_STATS_SUCCESS = 0
};

extern int   debug;
FILE  *where;

int                 ticks;

struct timeval      prev_time;
struct timeval      curr_time;

struct timeval      total_elapsed_time;
struct timeval      delta_elapsed_time;

int
sys_cpu_util_init(netsysstat_data_t *tsd) 
{
  where = stderr;
  int err;

  if ((tsd->num_cpus = sysconf(_SC_NPROCESSORS_ONLN)) == -1) {
    return(LINUX_SYS_STATS_SYSCONF_FAILED);
  }

  if (proc_stat_fd < 0) {
    proc_stat_fd = open (PROC_STAT_FILE_NAME, O_RDONLY, NULL);
    if (proc_stat_fd < 0) {
      fprintf (stderr, "Cannot open %s!\n", PROC_STAT_FILE_NAME);
      return(LINUX_SYS_STATS_PROCSTAT_OPEN_FAILED);
    };
  };

  if (!proc_stat_buf) {
    proc_stat_buflen = N_CPU_LINES (tsd->num_cpus) * CPU_LINE_LENGTH;
    proc_stat_buf = (char *)malloc (proc_stat_buflen);
    if (!proc_stat_buf) {
      fprintf (stderr, "Cannot allocate buffer memory!\n");
      close(proc_stat_fd);
      return(LINUX_SYS_STATS_MALLOC_FAILED);
    };
  };
  return LINUX_SYS_STATS_SUCCESS;
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
		      struct timeval *time,
		      netsysstat_data_t *tsd)
{

  int i,space,records;
  char *p = proc_stat_buf;
  char cpunam[64];
  uint64_t nicetime;

  lseek (proc_stat_fd, 0, SEEK_SET);
  read (proc_stat_fd, p, proc_stat_buflen);
  
  if (debug) {
    fprintf(where,"proc_stat_buf %s\n",p);
    fflush(where);
  }
  /* Skip first line (total) on SMP */
  if (tsd->num_cpus > 1) p = strchr (p, '\n');
  
  
  for (i = 0; i < tsd->num_cpus; i++) {
    records = sscanf(proc_stat_buf,
		     "%s %lld %lld %lld %lld",
		     cpunam,
		     res[i].user,
		     nicetime,
		     res[i].kernel,
		     res[i].idle);
    res[i].user += nicetime;
    res[i].other = 0;
    res[i].interrupt = 0;

    if(debug) {
      fprintf(where,
	      "\tidle[%d] = 0x%"PRIx64" ",
	      i,
	      res[i].idle);
      fprintf(where,
	      "user[%d] = 0x%"PRIx64" ",
	      i,
	      res[i].user);
      fprintf(where,
	      "kern[%d] = 0x%"PRIx64" ",
	      i,
	      res[i].kernel);
      fflush(where);
      fprintf(where,
	      "intr[%d] = 0x%"PRIx64"\n",
	      i,
	      res[i].interrupt);
    }
    p = strchr(p, '\n');
  }
}
