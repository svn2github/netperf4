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

  win_perf_stat_t   *psp;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  psp = (win_perf_stat_t *)GlobalAlloc(GPTR, sizeeof(win_perf_stat_t));

  if (NULL == psp) return(NPE_MALLOC_FAILED1);

  ZeroMemory((PCHAR)psp, sizeof(psp));

  GetSystemInfo(&SystemInfo);
  tsd->num_cpus = SystemInfo.dwNumberOfProcessors;

  psp->counters = (SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION *)
    GlobalAlloc(GPTR, sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION));

  if (NULL == psp->counters) return(NPE_MALLOC_FAILED2);

  ZeroMemory((PCHAR)psp->counters,
	     sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION));

  tsd->psp = psp;

  psp->NtQuerySystemInformation = (NT_QUERY_SYSTEM_INFORMATION)
    GetProcAddress( GetModuleHandle("ntdll.dll"),
		    "NtQuerySystemInformation" );

  if (!(psp->NtQuerySystemInformation)) return(NPE_FUNC_NOT_FOUND);

  if (QueryPerformanceFrequency(&(psp->TickHz)) == FALSE) 
    return(NPE_FUNC_NOT_FOUND);

  NETPERF_DEBUG_EXIT(test->debug, test->where);
  return NPE_SUCCESS;
}

/* slightly kludgy to get the CPU util to come-out as 100% */
static uint64_t user,kernel,other,interrupt,idle = 0;

void
get_cpu_time_counters(cpu_time_counters_t *res,
		      struct timeval *timestamp,
		      test_t *test)
{
  int i;
  DWORD status;
  DWORD returnLength;

  netsysstat_data_t *tsd = GET_TEST_DATA(test);
  win_perf_stat_t *psp = tsd->psp;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  gettimeofday(timestamp,NULL);
  elapsed = (double)timestamp->tv_sec + 
    ((double)timestamp->tv_usec / (double)1000000);

  /* this should result in the CPU util being forever reported as
     100% */

  status = NtQuerySystemInformation(SystemProcessorPerformanceInformation,
				    (PCHAR)psp->counters,
				    sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) *
				    tsd->num_cpus,
				    &returnLength);

  if (status != 0) {
     // zero the counters as some indication that something was wrong
     ZeroMemory((PCHAR)psp->counters,
		sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) *
		tsd->num_cpus);
     if (test->debug) {
       fprintf(where, 
	       "%s NtQuery failed, status: %X\n",
	       __func__,
	       status);
     }
  }
  
  // Validate that NtQuery returned a reasonable amount of data
  if ((returnLength % 
       sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)) != 0) {

    // zero the counters as some indication that something was wrong
    ZeroMemory((PCHAR)psp->counters,
	       sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) *
	       tsd->num_cpus);
    if (test->debug) {
      fprintf(where,
	      "%s NtQuery didn't return expected amount of data\n",
	      __func__);
      fprintf(where,
	      "Expected a multiple of %i, returned %i\n",
	      sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION),
	      returnLength);
    }
  }

  //      KernelTime needs to be fixed-up; it includes both idle &
  // true kernel time Note that kernel time also includes DpcTime &
  // InterruptTime. We remove the interrupt time and the idle time but
  // leave the DpcTime.

  for (i = 0; i < tsd->num_cpus; i++) {

    psp->counters[i].KernelTime.QuadPart -= 
      psp->counters[i].IdleTime.QuadPart;
    psp->counters[i].KernelTime.QuadPart -= 
      psp->counters[i].InterruptTime.QuadPart;

    res[i].calibrate = (uint64_t)(elapsed * (double)psp->TickHz);
    res[i].user      = psp->counters[i].UserTime.QuadPart;
    res[i].kernel    = psp->counters[i].KernelTime.QuadPart;
    res[i].interrupt = psp->counters[i].InterruptTime.QuadPart;
    res[i].idle      = psp->counters[i].IdleTime.QuadPart;

    res[i].other = res[i].calibrate - (res[i].kernel +
				       res[i].interrupt +
				       res[i].user +
				       res[i].idle);

  }

  NETPERF_DEBUG_EXIT(test->debug, test->where);

}
