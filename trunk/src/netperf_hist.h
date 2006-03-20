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
#ifdef IRIX
#include <sys/time.h>
#endif /* IRIX */

#if !defined(NETLIB) && !defined(WANT_HISTOGRAM)

#define HISTOGRAM_VARS       /* variable declarations for histogram go here */
#define HIST_TIMESTAMP(time) /* time stamp call for histogram goes here */
#define HIST_ADD(h,t1,t2)    /* call to add data to histogram goes here */
#define HIST_STATS_NODE(h,n) /* call to get hist statistics node goes here */
#define HIST_REPORT(fd,h)    /* call to report histogram data goes here */

#define HIST_NEW()           NULL

typedef void *HIST;

#else

#ifdef WANT_HISTOGRAM

#ifdef HAVE_GETHRTIME
#define HISTOGRAM_VARS        hrtime_t time_one,time_two
#define HIST_ADD(h,t1,t2)     HIST_add_nano(h,t1,t2)
#else
#define HISTOGRAM_VARS        struct timeval time_one,time_two
#define HIST_ADD(h,t1,t2)     HIST_add(h,delta_micro(t1,t2))
#endif

#define HIST_TIMESTAMP(time)  netperf_timestamp(time)
#define HIST_NEW()            HIST_new()
#define HIST_STATS_NODE(h,n)  HIST_stats_node(h,n)
#define HIST_REPORT(fd,h)     HIST_report(fd,h)

typedef void *HIST;

#endif

#ifdef NETLIB

/* hist.h

   Given a time difference in microseconds, increment one of 91
   different buckets: 
   
   0 - 9 in increments of 100 nsecs
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
   Stephen Burger 2006-03-17 - extend to 100 nsec
*/

struct histogram_struct {
  int      hundred_nsec[10];
  int      unit_usec[10];
  int      ten_usec[10];
  int      hundred_usec[10];
  int      unit_msec[10];
  int      ten_msec[10];
  int      hundred_msec[10];
  int      unit_sec[10];
  int      ten_sec[10];
  int      ridiculous;
  int64_t  total;
};

typedef struct histogram_struct *HIST;

#endif

#ifndef _HIST_INCLUDED
#define _HIST_INCLUDED
   
/* 
   HIST_new - return a new, cleared histogram data type
*/

extern HIST HIST_new(void); 

/* 
   HIST_clear - reset a histogram by clearing all totals to zero
*/

extern void HIST_clear(HIST h);

/*
   HIST_add - add a time difference to a histogram. Time should be in
   microseconds. 
*/

#ifdef HAVE_GETHRTIME
extern void HIST_add_nano(register HIST h, hrtime_t *begin, hrtime_t *end);
#else
extern void HIST_add(register HIST h, int time_delta);
#endif

/* 
  HIST_stats_node - create a histogram statistics xml node to report 
  on the contents of a histogram. Second parameter is a descriptive
  name to be reported with the histogram.
*/

extern xmlNodePtr HIST_stats_node(HIST h, char *name);

/* 
  HIST_report - create an ASCII report on the contents of a histogram.
  prints to file fd the histogram statistics in the xml node.
*/

extern void HIST_report(FILE *fd, xmlNodePtr h);

/*
  netperf_timestamp - take a timestamp suitable for use in a histogram.
*/

#ifdef HAVE_GETHRTIME
extern void netperf_timestamp(hrtime_t *timestamp);
#else
extern void netperf_timestamp(struct timeval *timestamp);
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
extern int delta_micro(hrtime_t *begin, hrtime_t *end);
extern int delta_milli(hrtime_t *begin, hrtime_t *end);
#else
extern int delta_micro(struct timeval *begin, struct timeval *end);
extern int delta_milli(struct timeval *begin, struct timeval *end);
#endif

#endif

#endif

