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

#include "netperf.h"

int
set_thread_locality(void  *threadid, char *loc_type, char *loc_value, int debug, FILE *where) {
  NETPERF_DEBUG_ENTRY(debug,where);
  if (debug) {
    fprintf(where,
	    "No call to set CPU affinity available, request ignored.\n");
    fflush(where);
  }
  NETPERF_DEBUG_EXIT(debug,where);
  return(NPE_SUCCESS);
}

int
set_test_locality(test_t *test, char *loc_type, char *loc_value)
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

int 
set_own_locality(char *loc_type, char *loc_value, int debug, FILE *where) {
  int my_id = 0;

  return(set_thread_locality((void *)&my_id,
			     loc_type,
			     loc_value,
			     debug,
			     where));
}
