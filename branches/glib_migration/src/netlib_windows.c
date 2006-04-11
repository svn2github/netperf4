static char netlib_specific_id[]="\
@(#)(c) Copyright 2006, Hewlett-Packard Company, $Id: netlib_none.c 133 2006-04-06 21:06:23Z raj $";

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

#include <windows.h>

#include "netperf.h"

int
set_thread_locality(void  *threadid, char *loc_type, char *loc_value, int debug, FILE *where) {
  int err, value;
  HANDLE thread_id;
  ULONG_PTR AffinityMask;

  NETPERF_DEBUG_ENTRY(debug,where);

  err = NPE_SUCCESS;
  if (threadid) {
    thread_id = *(HANDLE *)(threadid);
    value = atoi(loc_value);
    
    AffinityMask = (ULONG_PTR)1 << value;
    
    /* the netperf2 code had checks against the process affinity mask
       and if the desired CPU was not in that set the call to would
       fail.  I suspect the SetThreadAffinityMask would fail anyway so
       that seems unnecessary? raj 2006-04-11 */
    
    if (!SetThreadAffinityMask(thread_id,AffinityMask)) {
      
      if (debug) {
	fprintf(where,
		"%s's call to SetThreadAffinityMask value %d returned error\n",
		__func__,
		value);
	fflush(where);
      }
      err = NPE_SET_THREAD_LOCALITY_FAILED;
    }
  }
  else {
    /* we are relying on "proper" thread CPU affinity inheritence
       because it wasn't at all clear how to go about creating a
       "proper" handle by which one thread could reference another, so
       we set the native thread id to null in launch_pad */
    err = NPE_SUCCESS;
  }

  NETPERF_DEBUG_EXIT(debug,where);
  return(err);
}

int
set_test_locality(test_t *test, char *loc_type, char *loc_value)
{

  int err = -1;

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

  HANDLE my_id;

  /* we can use GetCurrentThread() here because the HANDLE is going to
     be references only in the context of the current thread */
  my_id = GetCurrentThread();

  return(set_thread_locality((void *)&my_id,
			     loc_type,
			     loc_value,
			     debug,
			     where));
}

/* to clear our thread locality, we will simply set it back to the
   process locality */

int
clear_own_locality(char *loc_type, int debug, FILE *where){
  int err;

  ULONG_PTR ProcessAffinityMask;
  ULONG_PTR SystemAffinityMask;

  err = NPE_SET_THREAD_LOCALITY_FAILED;
  if (GetProcessAffinityMask(GetCurrentProcess(),
			     &ProcessAffinityMask,
			     &SystemAffinityMask)); {
    /* OK, got the process mask, apply it to the thread */
    if (SetThreadAffinityMask(GetCurrentThread(),
			      ProcessAffinityMask)) {
      /* oh frabjous day, it worked */
      err = NPE_SUCCESS;
    }
  }
  return(err);
}
