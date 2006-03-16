/* Copyright 2005, Hewlett-Packard Company */

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

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

/* hist.h

   Given a time difference in microseconds, increment one of 81
   different buckets: 
   
   0 - 9 in increments of 1 usec
   0 - 9 in increments of 10 usecs
   0 - 9 in increments of 100 usecs
   0 - 9 in increments of 1 msec
   0 - 9 in increments of 10 msecs
   0 - 9 in increments of 100 msecs
   0 - 9 in increments of 1 sec
   0 - 9 in increments of 10 sec
   > 100 secs
   
   This will allow any time to be recorded to within an accuracy of
   10%, and provides a compact representation for capturing the
   distribution of a large number of time differences (e.g.
   request-response latencies).
   
   Colin Low  10/6/93
   Rick Jones 2004-06-15 - extend to 1 and 10 usec
*/
#ifndef _HIST_INCLUDED
#define _HIST_INCLUDED

#ifdef IRIX
#include <sys/time.h>
#endif /* IRIX */
   
struct histogram_struct {
  int unit_usec[10];
  int ten_usec[10];
  int hundred_usec[10];
  int unit_msec[10];
  int ten_msec[10];
  int hundred_msec[10];
  int unit_sec[10];
  int ten_sec[10];
  int ridiculous;
  int total;
};

typedef struct histogram_struct *HIST;

/* 
   HIST_new - return a new, cleared histogram data type
*/

HIST HIST_new(void); 

/* 
   HIST_clear - reset a histogram by clearing all totals to zero
*/

void HIST_clear(HIST h);

/*
   HIST_add - add a time difference to a histogram. Time should be in
   microseconds. 
*/

void HIST_add(register HIST h, int time_delta);

/* 
  HIST_report - create an ASCII report on the contents of a histogram.
  Currently printsto standard out 
*/

void HIST_report(HIST h);

/*
  HIST_timestamp - take a timestamp suitable for use in a histogram.
*/

#ifdef HAVE_GETHRTIME
void HIST_timestamp(hrtime_t *timestamp);
#else
void HIST_timestamp(struct timeval *timestamp);
#endif

/*
  delta_micro - calculate the difference in microseconds between two
  timestamps
  delta_milli - calculate the difference in milliseconds between two
  timestamps
  perhaps these things should be moved into netperf.h since they aren't
  limited to WANT_HISTOGRAM?  raj 2005-12-09
*/
#ifdef HAVE_GETHRTIME
int delta_micro(hrtime_t *begin, hrtime_t *end);
int delta_milli(hrtime_t *begin, hrtime_t *end);
#else
int delta_micro(struct timeval *begin, struct timeval *end);
int delta_milli(struct timeval *begin, struct timeval *end);
#endif

#endif

