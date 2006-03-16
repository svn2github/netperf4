static char netlib_specific_id[]="\
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "netperf.h"
#include "netlib.h"


#ifdef HAVE_SCHED_SETAFFINITY
int
set_thread_locality(test_t *test, char *loc_type, char *loc_value)
{
  int   err = -1;
  int   value;
  char *err_str;
  cpu_set_t      mask;


  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  CPU_ZERO(&mask);

  /* OK, right off the bat I see we have a potential issue with > 32
     CPUs if we just use atoi() so we'll need something else at some
     point. raj 2006-01-24 */

  value  = atoi(loc_value);

  CPU_SET(value, &mask);

  /* under linux, for now at least, all the flavors of affinity are
     the same. we keep the strucutre from the HP-UX version just for
     grins. raj 2006-01-24 */

  if (strcmp(loc_type,"PROC") == 0) {
    err  = sched_setaffinity(test->thread_id,
			     sizeof(mask),
			     &mask);
  }
  else if (strcmp(loc_type,"LDOM") == 0) {
    err  = sched_setaffinity(test->thread_id,
			     sizeof(mask),
			     &mask);
  }
  else if (strcmp(loc_type,"PSET") == 0) {
    err  = sched_setaffinity(test->thread_id,
			     sizeof(mask),
			     &mask);
  }
  if (err) {
    if (err == EINVAL) {
      err_str = "Invalid locality value";
    }
    if (err == EPERM) {
      err_str = "Netserver Permission Failure";
    }
    if (err == ESRCH) {
      err_str = "Invalid thread id";
    }
    if (err == EFAULT) {
      err_str = "Invalid memory address";
    }
    if (err == -1) {
      err_str = "Invalid locality type";
    }
    fprintf(test->where,
            "%s: failed to set locality %s\n",
            __func__,
            err_str);
    fflush(test->where);
    err = NPE_SET_THREAD_LOCALITY_FAILED;
  }
  else {
    err = NPE_SUCCESS;
  }
  NETPERF_DEBUG_EXIT(test->debug,test->where);
  return(err);
}
#else
int
set_thread_locality(test_t *test, char *loc_type, char *loc_value)
{
  NETPERF_DEBUG_ENTRY(test->debug,test->where);
  if (test->debug) {
    fprintf(test->where,
	    "No call to set CPU affinity available, request ignored.\n");
    fflush(test->where);
  }
  NETPERF_DEBUG_EXIT(test->debug,test->where);
  return(NPE_SUCCESS);
}
#endif
