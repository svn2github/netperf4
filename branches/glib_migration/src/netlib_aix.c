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

#include <sys/processor.h>
#include <sys/thread.h>

#include "netperf.h"

int
set_thread_locality(void  *threadid, char *loc_type, char *loc_value, int debug, FILE *where) {
  int err, value;
  tid_t thread_id;

  NETPERF_DEBUG_ENTRY(debug,where);

  thread_id = *(tid_t *)(threadid);
  value = atoi(loc_value);
  err = bindprocessor(BINDTHREAD, thread_id, value);

  if (debug) {
	fprintf(where,
		"%s's call to bindprocessor with BINDTHREAD thread %d and value %d returned %d errno %d\n",
		__func__,
                thread_id,
		value,
                err,
                errno);
	fflush(where);
  }

  NETPERF_DEBUG_EXIT(debug,where);
  return(NPE_SUCCESS);
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
  tid_t my_id;

  my_id = thread_self();

  return(set_thread_locality((void *)&my_id,
			     loc_type,
			     loc_value,
			     debug,
			     where));
}

int
clear_own_locality(char *loc_type, int debug, FILE *where){
  int err;

  err = bindprocessor(BINDTHREAD, thread_self(), PROCESSOR_CLASS_ANY);

  return(NPE_SUCCESS);
}
