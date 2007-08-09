/*

Copyright (c) 2005,2006 Hewlett-Packard Company 

$Id$

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

#ifndef _NETSYSSTATS_H
#define _NETSYSSTATS_H
/* Macros for accessing fields in the global netperf structures. */
#define SET_TEST_STATE(state)             test->new_state = state
#define GET_TEST_STATE                    test->new_state
#define CHECK_REQ_STATE                   test->state_req
#define GET_TEST_DATA(test)               test->test_specific_data

/* global variables are used to keep copies of system statistics since
   only one instance of the test should be running on any system  */

/* still, one wonders if it might be more "consistent" to use the
   test-specific data section of the test structure... raj
   2005-06-10 */

typedef struct cpu_time_counters {
  uint64_t calibrate; /* the number by which all the rest should be
                         divided */
  uint64_t idle;      /* the number of time units in the idle state */
  uint64_t user;      /* the number of time units in the user state */
  uint64_t kernel;    /* the number of time units in the kernel state */
  uint64_t interrupt; /* the number of time units in the interrupt
                         state */
  		      /* On linux, this is the number of time units 
		       * spent servicing hardware interrupts. */
  /* PN: 06/27/2007.
   * Other cpu stats we are interested in
   */
  uint64_t nice;      /* the number of time units in nice state
			 -- different from user */
  uint64_t iowait;    /* the number of time units in wait state
			 for I/O completion */
  uint64_t softirq;   /* the number of time units spent processing
			 softirqs */
			 
  uint64_t other;     /* the number of time units in other states
                         and/or any slop we have */

  /* PN: 07/12/2007 */
  uint64_t total_intr; /* the total number of interrupts (all kinds) 
			  serviced by the CPU. On linux, this value
			  is collected from /proc/interrupts */
  
} cpu_time_counters_t;

typedef struct netsysstat_data {
  int                   num_cpus;
  char                 *method;
  struct timeval        prev_time;
  struct timeval        curr_time;
  struct timeval        total_elapsed_time;
  struct timeval        delta_elapsed_time;
  cpu_time_counters_t  *total_sys_counters;
  cpu_time_counters_t  *starting_cpu_counters;
  cpu_time_counters_t  *ending_cpu_counters;
  cpu_time_counters_t  *delta_cpu_counters;
  cpu_time_counters_t  *total_cpu_counters;
  void                 *psd;         /* a pointer to platform specific data */
} netsysstat_data_t;

extern void get_cpu_time_counters(cpu_time_counters_t *res,
				  struct timeval *timestamp,
				  test_t *test);

extern int sys_cpu_util_init(test_t *test);
#endif
