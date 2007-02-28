static char netlib_specific_id[]="\
@(#)(c) Copyright 2006 Hewlett-Packard Company, $Id$";

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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include <sys/pset.h>
#include <sys/lwp.h>

#include "netperf.h"
#include "netlib.h"

int
set_thread_locality(void *threadid, char *loc_type, char *loc_value, int debug, FILE *where)
{
  int   err = -1;
  int   value;
  char *err_str;

  lwpid_t thread_id;

  NETPERF_DEBUG_ENTRY(debug,where);

  thread_id = *(lwpid_t *)(threadid);

  value  = atoi(loc_value);
  if (strcmp(loc_type,"PROC") == 0) {
    err  = processor_bind(P_LWPID,
                          thread_id,
                          value,
			  NULL);
  }
  else if (strcmp(loc_type,"LDOM") == 0) {
    /* not quite sure what we should do here, so for now we will do
       nothing at all. raj 2006-06-28 */
  }
  else if (strcmp(loc_type,"PSET") == 0) {
    /* figures that Sun wouldn't keep the order of arguments
       consistent with processor_bind... raj 2006-06-28 */
    err = pset_bind(value,
		    P_LWPID,
		    thread_id,
		    NULL);
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
    if (err == -1) {
      err_str = "Invalid locality type";
    }
    fprintf(where,
            "%s: failed to set locality %s\n",
            __func__,
            err_str);
    fflush(where);
    err = NPE_SET_THREAD_LOCALITY_FAILED;
  }
  else {
    err = NPE_SUCCESS;
  }

  NETPERF_DEBUG_EXIT(debug,where);

  return(err);
}

int
set_test_locality(test_t *test, char *loc_type, char *loc_value)
{
  int   err = -1;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  err = set_thread_locality(test->native_thread_id_ptr,
			    loc_type,
			    loc_value,
			    test->debug,
			    test->where);

  NETPERF_DEBUG_EXIT(test->debug,test->where);

  return(err);
}

int 
set_own_locality(char *loc_type, char *loc_value, int debug, FILE *where) {
  lwpid_t my_id;

  my_id = _lwp_self();

  return(set_thread_locality((void *)&my_id,
			     loc_type,
			     loc_value,
			     debug,
			     where));
}

int
clear_own_locality(char *loc_type, int debug, FILE *where){

  int err = -1;

  NETPERF_DEBUG_ENTRY(debug,where);

  if (strcmp(loc_type,"PROC") == 0) {
    err = processor_bind(P_LWPID,
			 P_MYID,
			 PBIND_NONE,
			 NULL);
  }
  else if (strcmp(loc_type,"LDOM") == 0) {
    /* as I know not what to do for a locality domain under solairs, I
       will do nothing. */
  }
  else if (strcmp(loc_type,"PSET") == 0) {
    /* well, it seems as there is no "FLOAT" pset identifier, which
       means we could only go back to where we were before, assuming
       of course, we actually knew where that happened to be. at the
       moment, we have no clue, so we will do nothing. raj
       2006-04-11 */
    err = pset_bind(PS_NONE,
		    P_LWPID,
		    P_MYID,
		    NULL);
  }

  return(NPE_SUCCESS);

}
