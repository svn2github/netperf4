/* 
 * PN: New source file to gather TCP statistics on linux
 */

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

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif 

#include <errno.h>

#include "netperf.h"
#include "nettcpstats.h"

#define PROC_NETSTAT_FILE_NAME "/proc/net/netstat"

/* Desired fields from /proc/net/netstat line 
 * count starts from 0.
 */
#define TCPLOSS		41
#define TCPLOSTRTX	42
#define TCPFASTRTX	46
#define TCPTIMEOUT	49

/* length of the initial "TcpExt: " */
#define PROC_NETSTAT_FIRST_ENTRY_SIZE	10

#define NUM_PROC_NETSTAT_ENTRIES	65

/* Length of /proc/net/netstat first line */
#define PROC_NETSTAT_FIRST_LINE_LENGTH	\
	( NUM_PROC_NETSTAT_ENTRIES  * 30  \
	  + PROC_NETSTAT_FIRST_ENTRY_SIZE )

/* Length of second line */
#define PROC_NETSTAT_LINE_LENGTH \
	((NUM_PROC_NETSTAT_ENTRIES \
	  * sizeof (long unsigned) \
	  * + PROC_NETSTAT_FIRST_ENTRY_SIZE))

/* /proc/net/netstat column/entry separator */
#define PROC_NETSTAT_COL_SEP_CHAR	' '

static int proc_netstat_fd = -1;
static char* proc_netstat_buf = NULL;
static int proc_netstat_buflen = 0;
static int netstats_offset = -1;

enum {
  LINUX_TCP_STATS_PROC_OPEN_FAILED = -2,
  LINUX_TCP_STATS_MALLOC_FAILED = -1,
  LINUX_TCP_STATS_SUCCESS = 0
};

void
tcp_stat_clear(test_t *test)
{

  NETPERF_DEBUG_ENTRY(test->debug,test->where);
  if (proc_netstat_buf) free(proc_netstat_buf);
  proc_netstat_buf = NULL;

  if (proc_netstat_fd > 0) close(proc_netstat_fd);
  NETPERF_DEBUG_EXIT(test->debug,test->where);

}
int
tcp_stat_init(test_t *test)
{

  nettcpstat_data_t *tsd = GET_TEST_DATA(test);
  char temp[PROC_NETSTAT_FIRST_LINE_LENGTH];
  char *p = temp;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  if (proc_netstat_fd < 0) {
    proc_netstat_fd = open (PROC_NETSTAT_FILE_NAME, O_RDONLY, NULL);
    if (proc_netstat_fd < 0) {
      fprintf (stderr, "Cannot open %s!\n", PROC_NETSTAT_FILE_NAME);
      return(LINUX_TCP_STATS_PROC_OPEN_FAILED);
    };
  };


  if (!proc_netstat_buf) {

    proc_netstat_buflen = PROC_NETSTAT_LINE_LENGTH;
    proc_netstat_buf = (char *)malloc (proc_netstat_buflen + 1);
    proc_netstat_buf[PROC_NETSTAT_LINE_LENGTH] = '\0';
    if (!proc_netstat_buf) {
      fprintf (stderr, "Cannot allocate buffer memory!\n");
      close(proc_netstat_fd);
      return(LINUX_TCP_STATS_MALLOC_FAILED);
    };

  };

  /* Find the offset for 2nd line -- stats start here */
  lseek (proc_netstat_fd, 0, SEEK_SET);
  read (proc_netstat_fd, p, PROC_NETSTAT_FIRST_LINE_LENGTH);

  p = strchr(p, '\n');

  /* For subsequent reads, lseek using netstats_offset */
  netstats_offset = p - temp + 1; 

  fprintf(test->where, "temp = %p, p = %p, netstats_offset = %d\n",
		  temp, p, netstats_offset);

  NETPERF_DEBUG_EXIT(test->debug,test->where);
  return LINUX_TCP_STATS_SUCCESS;
}

void
get_tcp_stat_counters(tcp_stat_counters_t *res,
                      test_t *test)
{

  int i = 0;
  nettcpstat_data_t *tsd = GET_TEST_DATA(test);
  char *p = proc_netstat_buf;
  unsigned long value;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  lseek (proc_netstat_fd, 0, SEEK_SET);
  lseek (proc_netstat_fd, netstats_offset, SEEK_SET);
  
  read (proc_netstat_fd, p, proc_netstat_buflen);
  
  /* fprintf(test->where, "After read -- p = %s\n", p); */
 
  /* PN: Start reading the ridiculously long stats line */

  do {
        sscanf(p, "%lu ", &value);
			  
	switch (i)
	{
	  case TCPLOSS: 
	    res->tcpLoss	= value;
	    fprintf(test->where, "%s: tcpLoss = %lu", __func__, value);
	    fflush(test->where);
	    break;
	  case TCPLOSTRTX:
	    res->tcpLostRtxmts	= value;
	    fprintf(test->where, "\ttcpLostRtxmts = %lu", value);
	    fflush(test->where);
	    break;
	  case TCPFASTRTX:
	    res->tcpFastRtxmts	= value;
	    fprintf(test->where, "\ttcpFastRtxmts = %lu", value);
	    fflush(test->where);
	    break;
	  case TCPTIMEOUT:
	    res->tcpTimeouts	= value;
	    fprintf(test->where, "\ttcpTimeouts = %lu\n", value);
	    fflush(test->where);
	    break;
	}

	i++;
        p = strchr(p, PROC_NETSTAT_COL_SEP_CHAR);
        p++;

  } while (i <= TCPTIMEOUT); /* ideal: i < NUM_PROC_NETSTAT_ENTRIES */

  NETPERF_DEBUG_EXIT(test->debug,test->where);
}
