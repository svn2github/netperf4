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

/* Max length of network device name */
#define MAX_DEVNAME_LENGTH	10

/* Network device statistics
 */
typedef struct netdev_stat_counters {


  /* Look at /usr/include/linux/netdevice.h for more details */

  char devName[MAX_DEVNAME_LENGTH];

  uint64_t rcvPackets;		/* the number of packets received by device */
  uint64_t rcvDrops;		/* the number of received packets dropped 
  			   	   due to lack of linux buffer space */ 
  uint64_t rcvErrors;		/* the number of packets received with errors */

  uint64_t trxPackets;		/* the number of packets transmitted by device */
  uint64_t trxDrops;		/* the number of packets dropped  due to 
  			   	   lack of linux buffer space */ 
  uint64_t trxErrors;		/* the number of packet transmit errors */ 

  uint64_t trxCollisions;	/* the number of collisions detected */
  
} netdev_stat_counters_t;

typedef struct netdevstat_data {

  int 			  num_network_interfaces;
  netdev_stat_counters_t  *starting_netdev_counters;
  netdev_stat_counters_t  *ending_netdev_counters;
  netdev_stat_counters_t  *delta_netdev_counters;
} netdevstat_data_t;

extern void get_netdev_stat_counters(netdev_stat_counters_t *res,
				  test_t *test);

extern int netdev_stat_init(test_t *test);

extern void netdev_stat_clear(test_t *test);
