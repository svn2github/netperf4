char netsysstats_windows[]="\
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

#include <process.h>
#include <windows.h>

//
// System CPU time information class.
// Used to get CPU time information.
//
//     SDK\inc\ntexapi.h 
// Function x8:   SystemProcessorPerformanceInformation
// DataStructure: SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION 
//

#define SystemProcessorPerformanceInformation 0x08

typedef struct 
{
        LARGE_INTEGER   IdleTime;
        LARGE_INTEGER   KernelTime;
        LARGE_INTEGER   UserTime;
        LARGE_INTEGER   DpcTime;
        LARGE_INTEGER   InterruptTime;
        LONG            InterruptCount;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

//
// Calls to get the information
//
typedef ULONG (__stdcall *NT_QUERY_SYSTEM_INFORMATION)( 
	ULONG SystemInformationClass, 
	PVOID SystemInformation, 
	ULONG SystemInformationLength,
	PULONG ReturnLength);

_inline LARGE_INTEGER ReadPerformanceCounter(VOID)
{
        LARGE_INTEGER Counter;
        QueryPerformanceCounter(&Counter);

        return(Counter);
}       // ReadperformanceCounter


typedef struct perf_stat {
  NT_QUERY_SYSTEM_INFORMATION NtQuerySystemInformation;
  LARGE_INTEGER TickHz;
  SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION *counters;
} win_perf_stat_t;


#include "netperf.h"
#include "netsysstats.h"

int
sys_cpu_util_init(test_t *test) 
{

  SYSTEM_INFO SystemInfo;

  netsysstat_data_t *tsd = GET_TEST_DATA(test);

  win_perf_stat_t   *psd;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  psd = (win_perf_stat_t *)GlobalAlloc(GPTR, sizeof(win_perf_stat_t));

  if (NULL == psd) return(NPE_MALLOC_FAILED1);

  ZeroMemory((PCHAR)psd, sizeof(psd));

  GetSystemInfo(&SystemInfo);
  tsd->num_cpus = SystemInfo.dwNumberOfProcessors;

  psd->counters = (SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION *)
    GlobalAlloc(GPTR, sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION));

  if (NULL == psd->counters) return(NPE_MALLOC_FAILED2);

  ZeroMemory((PCHAR)psd->counters,
	     sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION));

  tsd->psd = psd;

  psd->NtQuerySystemInformation = (NT_QUERY_SYSTEM_INFORMATION)
    GetProcAddress( GetModuleHandle("ntdll.dll"),
		    "NtQuerySystemInformation" );

  if (!(psd->NtQuerySystemInformation)) return(NPE_FUNC_NOT_FOUND);

  if (QueryPerformanceFrequency(&(psd->TickHz)) == FALSE) 
    return(NPE_FUNC_NOT_FOUND);

  NETPERF_DEBUG_EXIT(test->debug, test->where);
  return NPE_SUCCESS;
}

void
get_cpu_time_counters(cpu_time_counters_t *res,
		      struct timeval *timestamp,
		      test_t *test)
{
  int i;
  DWORD status;
  DWORD returnLength;
  double elapsed;

  netsysstat_data_t *tsd = GET_TEST_DATA(test);
  win_perf_stat_t *psd = tsd->psd;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  // it has been suggested that we might want to sit and spin until
  // the next wall-clock tick before we snap the CPU counters. that
  // would be done via:
  // Spin until clock interrupt boundary
  status = GetTickCount();
  do {
  } while (status == GetTickCount());

  // however, I'm concerned that sitting and spinning might add too
  // much to the CPU counters.  the same source that suggested the sit
  // and spin pointed-out that if our intervals are more than say 10
  // seconds, it may not really matter. food for thought.
  // in the end I decided to go that way since the wall clock time
  // under Windows - retrieved via the gettimeofday() (really
  // g_get_current_time, which calls GetSystemTimeAsFileTime only
  // increments in 10 ms increments!?!!!

  gettimeofday(timestamp,NULL);
  elapsed = (double)timestamp->tv_sec + ((double)timestamp->tv_usec /
                                         (double)1000000);

  // it has been suggested that we might want to sit and spin until
  // the next wall-clock tick before we snap the CPU counters. that
  // would be done via:
  //	// Spin until clock interrupt boundary
  //	dwEnd = GetTickCount();
  //	do {
  //	} while (dwEnd == GetTickCount());
  // however, I'm concerned that sitting and spinning might add too
  // much to the CPU counters.  the same source that suggested the sit
  // and spin pointed-out that if our intervals are more than say 10
  // seconds, it may not really matter. food for thought.

  status = (psd->NtQuerySystemInformation)
    (SystemProcessorPerformanceInformation,
     (PCHAR)psd->counters,
     sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) *
     tsd->num_cpus,
     &returnLength);

  if (status != 0) {
     // zero the counters as some indication that something was wrong
     ZeroMemory((PCHAR)psd->counters,
		sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) *
		tsd->num_cpus);
     if (test->debug) {
       fprintf(test->where, 
	       "%s NtQuery failed, status: %X\n",
	       __func__,
	       status);
     }
  }
  
  // Validate that NtQuery returned a reasonable amount of data
  if ((returnLength % 
       sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)) != 0) {

    // zero the counters as some indication that something was wrong
    ZeroMemory((PCHAR)psd->counters,
	       sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) *
	       tsd->num_cpus);
    if (test->debug) {
      fprintf(test->where,
	      "%s NtQuery didn't return expected amount of data\n",
	      __func__);
      fprintf(test->where,
	      "Expected a multiple of %i, returned %i\n",
	      sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION),
	      returnLength);
    }
  }

  //      KernelTime needs to be fixed-up; it includes both idle &
  // true kernel time Note that kernel time also includes DpcTime &
  // InterruptTime. We remove the interrupt time and the idle time but
  // leave the DpcTime.

  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sysinfo/base/ntquerysysteminformation.asp
  // asserts that each CPU tick represents 1/100ths of a nanosecond
  // (well, at least that is the claim as this is being written.
  // However, others have said, and empirical evidence suggests that
  // the ticks really represent 100 nanoseconds (eg 10 MHz).  So, for
  // our calibration value we take the wall-clock time and multiply it
  // by 10MHz to use as the calibration value.  raj 2006-04-12 

  for (i = 0; i < tsd->num_cpus; i++) {

    psd->counters[i].KernelTime.QuadPart -= 
      psd->counters[i].IdleTime.QuadPart;
    psd->counters[i].KernelTime.QuadPart -= 
      psd->counters[i].InterruptTime.QuadPart;

    res[i].calibrate = (uint64_t)(elapsed * 10000000.0);
    res[i].user      = psd->counters[i].UserTime.QuadPart;
    res[i].kernel    = psd->counters[i].KernelTime.QuadPart;
    res[i].interrupt = psd->counters[i].InterruptTime.QuadPart;
    res[i].idle      = psd->counters[i].IdleTime.QuadPart;

    res[i].other = res[i].calibrate - (res[i].kernel +
				       res[i].interrupt +
				       res[i].user +
				       res[i].idle);

    if (test->debug) {
      fprintf(test->where,
	      "\tTickHz 0x%"PRIx64" ",
              psd->TickHz.QuadPart);
      fprintf(test->where,
	      "\tcalibrate[%d] = 0x%"PRIx64" ",
	      i,
	      res[i].calibrate);
      fprintf(test->where,
	      "\tidle[%d] = 0x%"PRIx64" ",
	      i,
	      res[i].idle);
      fprintf(test->where,
	      "user[%d] = 0x%"PRIx64" ",
	      i,
	      res[i].user);
      fprintf(test->where,
	      "kern[%d] = 0x%"PRIx64" ",
	      i,
	      res[i].kernel);
      fflush(test->where);
      fprintf(test->where,
	      "intr[%d] = 0x%"PRIx64"\n",
	      i,
	      res[i].interrupt);
    }
  }

  NETPERF_DEBUG_EXIT(test->debug, test->where);

}
