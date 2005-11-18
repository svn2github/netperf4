/*        Copyright (C) 2005 Hewlett-Packard Company */

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

/* This file contains the test-specific definitions for netperf4's DNS
   tests */

enum {
  SEND_CALLS = 0,
  BYTES_SENT,
  RECV_CALLS,
  BYTES_RECEIVED,
  TRANS_SENT,
  TRANS_RECEIVED,
  CONNECT_CALLS,
  ACCEPT_CALLS,
  DNS_MAX_COUNTERS
};

/* we don't have an ID field because that will be implicit in the
   array index - at least at first until we decide that is consuming
   too much memory or something :) raj 2005-11-17 */

/* I wonder if we can ass-u-me that checking against the request id is
   sufficient or if we need to also check against stuff like the type
   and the class? raj 2005-11-17 */

  /* until we get the gethrtime() stuff done, this will be a struct
     timeval and... this should probably really be in netperf.h... */
#define NETPERF_TIMESTAMP_T struct timeval
#define NETPERF_TIME_STAMP(s) gettimeofday(&(s),NULL);

typedef struct dns_request_status {
  unsigned short active; /* was there a query sent with this id for
		 which we are awaiting a response */
  unsigned short success; /* should the request have been successful
			     or not */
  NETPERF_TIMESTAMP_T sent_time;
} dns_request_status_t;

typedef struct  dns_test_data {
  /* address information */
  struct addrinfo *locaddr;        /* local address informtion */
  struct addrinfo *remaddr;        /* remote address informtion */

  FILE            *request_source; /* pointer to file from which we
				      pull requests */

  int              query_socket;   /* socket for sending queries */

  /* misc parameters */
  int   use_tcp;        /* should queries be sent on a tcp connection */
  int   no_delay;       /* do we disable the nagle algorithm for send */
  int   keepalive;      /* do we keep the TCP connection open */
  int   sbuf_size_ret;  /* send socket buffer size returned on creation */
  int   send_buf_size;  /* send socket buffer size */
  int   rbuf_size_ret;  /* receive socket buffer size returned on creation */
  int   recv_buf_size;  /* receive socket buffer size */

  uint16_t  request_id; /* this is used to match requests with
			   responses and serves as the index into the
			   outstanding queries array */

  dns_request_status_t outstanding_requests[UINT16_MAX];

  /* Statistics Counters - they should all be uint64_t */
  union {
    uint64_t  counter[DNS_MAX_COUNTERS];
    struct {
      uint64_t  queries_sent; /* the number of queries sent */
      uint64_t  query_bytes_sent;   /* their byte count */
      uint64_t  responses_received; /* the number of responses
				       received */
      uint64_t  response_bytes_received; /* their byte count */
      uint64_t  connect_calls;
    } named;
  } stats;
  struct timeval  elapsed_time;
  struct timeval  prev_time;
  struct timeval  curr_time;

  /* Place for HISTOGRAM fields */
  HIST time_hist;
} dns_data_t;

typedef struct  dns_results_data {
  int     max_count;
  int     print_test;
  int     print_run;
  FILE   *outfd;
  double *results;
  double *xmit_results;
  double *recv_results;
  double *trans_results;
  double *utilization;
  double *servdemand;
  double *run_time;
  double ave_time;
  double result_measured_mean;
  double result_confidence;
  double result_minimum;
  double result_maximum;
  double cpu_util_measured_mean;
  double cpu_util_confidence;
  double service_demand_measured_mean;
  double service_demand_confidence;
  double confidence;
  double sd_denominator;
  double results_start;  /* must be the last field in structure */
} dns_results_t;

/* Error codes to be used within nettest_dns */
enum {
  DNSE_MAX_ERROR = -32,
  DNSE_SOCKET_SHUTDOWN_FAILED,
  DNSE_BIND_FAILED,
  DNSE_GETADDRINFO_ERROR,
  DNSE_XMLSETPROP_ERROR,
  DNSE_XMLNEWNODE_ERROR,
  DNSE_NO_SOCKET_ARGS,
  DNSE_SOCKET_ERROR,
  DNSE_SETSOCKOPT_ERROR,
  DNSE_LISTEN_FAILED,
  DNSE_GETSOCKNAME_FAILED,
  DNSE_REQUESTED_STATE_INVALID,
  DNSE_ACCEPT_FAILED,
  DNSE_DATA_RECV_ERROR,
  DNSE_TEST_STATE_CORRUPTED,
  DNSE_CONNECT_FAILED,
  DNSE_DATA_CONNECTION_CLOSED_ERROR,
  DNSE_DATA_SEND_ERROR=-1,
  DNSE_SUCCESS = 0
};
