/*

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

/*
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
*/

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif 

#include <errno.h>

#include "netperf.h"
#include "netdevstats.h"

#define PROC_NETDEV_FILE_NAME "/proc/net/dev"

/* Desired counters from /proc/net/dev */
#define NETDEV_RCVPKTS		0
#define NETDEV_RCVERRS		1
#define NETDEV_RCVDRPS		2
#define NETDEV_TRXPKTS		3
#define NETDEV_TRXERRS		4
#define NETDEV_TRXDRPS		5
#define NETDEV_TRXCOLLS		6

#define NUM_NETDEV_ENTRIES	7

/* Set the length of /proc/net/dev header length to something huge */
#define PROC_NETDEV_HEADER_LENGTH	500

#define NUM_PROC_NETDEV_ENTRIES		16

/* Length of stats line */
#define PROC_NETDEV_LINE_LENGTH \
	((NUM_PROC_NETDEV_ENTRIES \
	  * sizeof (long unsigned) \
	  * + MAX_DEVNAME_LENGTH))

/* /proc/net/dev column/entry separator */
#define PROC_NETDEV_COL_SEP_CHAR	' '

static int proc_netdev_fd = -1;
static char* proc_netdev_buf = NULL;
static int proc_netdev_buflen = 0;
static int netdev_offset = -1;
static int num_network_interfaces = 0;

enum {
  LINUX_NETDEV_STATS_INTERFACES_FAILED	= -3,
  LINUX_NETDEV_STATS_PROC_OPEN_FAILED = -2,
  LINUX_NETDEV_STATS_MALLOC_FAILED = -1,
  LINUX_NETDEV_STATS_SUCCESS = 0
};

void
netdev_stat_clear(test_t *test)
{

  NETPERF_DEBUG_ENTRY(test->debug,test->where);
  if (proc_netdev_buf) free(proc_netdev_buf);
  proc_netdev_buf = NULL;

  if (proc_netdev_fd > 0) close(proc_netdev_fd);
  NETPERF_DEBUG_EXIT(test->debug,test->where);

}
int
netdev_stat_init(test_t *test)
{

  netdevstat_data_t *tsd = GET_TEST_DATA(test);
  char *temp = NULL;
  char *p = NULL;
  int line_size = 0, err = -1;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  if (proc_netdev_fd < 0) {
    proc_netdev_fd = open (PROC_NETDEV_FILE_NAME, O_RDONLY, NULL);
    if (proc_netdev_fd < 0) {
      fprintf (stderr, "Cannot open %s!\n", PROC_NETDEV_FILE_NAME);
      return(LINUX_NETDEV_STATS_PROC_OPEN_FAILED);
    };
  };

  if (PROC_NETDEV_LINE_LENGTH > PROC_NETDEV_HEADER_LENGTH) {
    temp = (char *)malloc (PROC_NETDEV_LINE_LENGTH + 1);
    temp[PROC_NETDEV_LINE_LENGTH] = '\0';
  }
  else {
    temp = (char *)malloc (PROC_NETDEV_HEADER_LENGTH + 1);
    temp[PROC_NETDEV_LINE_LENGTH] = '\0';
  }

  p = temp;

  /* Find the offset for 3rd line -- stats start here */
  lseek (proc_netdev_fd, 0, SEEK_SET);
  read (proc_netdev_fd, p, PROC_NETDEV_HEADER_LENGTH);

  p = strchr(p, '\n');
  p = p++;
  p = strchr(p, '\n');

  /* For subsequent reads, lseek using netdev_offset */
  netdev_offset = p - temp + 1; 

  fprintf(test->where, "temp = %p, p = %p, netdev_offset = %d\n",
		  temp, p, netdev_offset);
  fflush(test->where);

  /* Find number of network interfaces listed
   * Is there a sysctl for this number/any other way
   * to get this information? TODO...
   * num_network_interfaces = num lines in /proc/net/dev
   */
  lseek (proc_netdev_fd, netdev_offset, SEEK_SET);

  do {
    memset(temp, 0, strlen(temp));
    err = read(proc_netdev_fd, temp, PROC_NETDEV_LINE_LENGTH);
    if (err > 0) {
	p = temp;
	p = strchr(p, '\n');
	p++; /* next line starts here */
	line_size += (p - temp);
	num_network_interfaces++;

	/* next read should start from next line */
	lseek(proc_netdev_fd, netdev_offset + line_size, SEEK_SET);
    }
  } while (err > 0);

  if (!num_network_interfaces) {
    fprintf (stderr, "Cannot find out number of network interfaces!\n");
    close(proc_netdev_fd);
    free(temp);
    temp = NULL;
    return(LINUX_NETDEV_STATS_INTERFACES_FAILED);
  }
  else {
    fprintf (test->where, 
	  "Found %d network interfaces...\n", num_network_interfaces);
    fflush(test->where);
    tsd->num_network_interfaces = num_network_interfaces;
  }
  
  if (!proc_netdev_buf) {

    proc_netdev_buflen = (PROC_NETDEV_LINE_LENGTH * num_network_interfaces);
    proc_netdev_buf = (char *)malloc (proc_netdev_buflen + 1);
    proc_netdev_buf[proc_netdev_buflen] = '\0';
    if (!proc_netdev_buf) {
      fprintf (stderr, "Cannot allocate buffer memory!\n");
      close(proc_netdev_fd);
      free(temp);
      temp = NULL;
      return(LINUX_NETDEV_STATS_MALLOC_FAILED);
    };

  };

  free(temp);
  temp = NULL;

  NETPERF_DEBUG_EXIT(test->debug,test->where);
  return LINUX_NETDEV_STATS_SUCCESS;
}

void
get_netdev_stat_counters(netdev_stat_counters_t *res,
                      test_t *test)
{

  int i = 0, j = 0, k = 0;
  netdevstat_data_t *tsd = GET_TEST_DATA(test);
  char *p = proc_netdev_buf;
  unsigned long value;
  char devname[100]; 
  char *q = devname;
  char value_str[NUM_NETDEV_ENTRIES][20];
  char dummy_value[20];

  NETPERF_DEBUG_ENTRY(test->debug,test->where);
  memset(proc_netdev_buf, 0, proc_netdev_buflen);

  lseek (proc_netdev_fd, netdev_offset, SEEK_SET);
  read (proc_netdev_fd, p, proc_netdev_buflen);

  fprintf(test->where, "Netdev contents: %s\n", p);
  fflush(test->where);
  
  i = 0;
  do {
	 
    /* scan the interface name..
     * format -- "ifacename:bytes  errs "...
     * Why do they have so many kinds of field separators ??!! 
     */

    sscanf(p, "%s", devname);
    q = devname;
    q = strchr(q, ':');
    if ((q - devname) > MAX_DEVNAME_LENGTH) {
      devname[MAX_DEVNAME_LENGTH] = '\0' ;
    }
    else {
      devname[q - devname] = '\0';
    }

    strcpy(res[i].devName, devname);

    fprintf(test->where, "Interface: %d, devName = %s ", i, res[i].devName);
    fflush(test->where);

    /* Move to the first entry after interface name
     * I.e. move to bytes in "ifacename:bytes"
     */
    p = strchr(p, ':');
    p++;

    /* Entries for each interface in /proc/net/dev are separated by
     * multiple delimiters. It appears to be a very error prone process
     * to read them as numbers. The best option seems to be reading them
     * as strings.  
     */
    sscanf(p, "%s %s %s %s %s %s %s %s %s %s %s %s %s %s",
			dummy_value,
			value_str[0],
			value_str[1],
			value_str[2],
			dummy_value,
			dummy_value,
			dummy_value,
			dummy_value,
			dummy_value,
			value_str[3],
			value_str[4],
			value_str[5],
			dummy_value,
			value_str[6]);

    for(j = 0; j < NUM_NETDEV_ENTRIES; j++) {

      value = strtod(value_str[j], NULL);

      switch (j)
      {
          case NETDEV_RCVPKTS:
            res[i].rcvPackets     = value;
            fprintf(test->where, "\trcvPackets = %lu", value);
            fflush(test->where);
            break;
          case NETDEV_RCVERRS:
            res[i].rcvErrors      = value;
            fprintf(test->where, "\trcvErrors = %lu", value);
            fflush(test->where);
            break;
          case NETDEV_RCVDRPS:
            res[i].rcvDrops       = value;
            fprintf(test->where, "\trcvDrops = %lu", value);
            fflush(test->where);
            break;
          case NETDEV_TRXPKTS:
            res[i].trxPackets     = value;
            fprintf(test->where, "\ttrxPackets = %lu", value);
            fflush(test->where);
            break;
          case NETDEV_TRXERRS:
            res[i].trxErrors      = value;
            fprintf(test->where, "\ttrxErrors = %lu", value);
            fflush(test->where);
            break;
          case NETDEV_TRXDRPS:
            res[i].trxDrops       = value;
            fprintf(test->where, "\ttrxDrops = %lu", value);
            fflush(test->where);
            break;
          case NETDEV_TRXCOLLS:
            res[i].trxCollisions  = value;
            fprintf(test->where, "\ttrxCollisions = %lu\n", value);
            fflush(test->where);
        }
    }

    i++;
    
    /* Go to the next interface */
    if (i < num_network_interfaces) {
    	p = strchr(p, '\n');
    	p++;
    }

  } while ( i < num_network_interfaces);

  NETPERF_DEBUG_EXIT(test->debug, test->where);

}
