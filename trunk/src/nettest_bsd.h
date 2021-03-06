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

 /* This file contains the test-specific definitions for netperf4's BSD */
 /* sockets tests */

struct ring_elt {
  struct ring_elt *next;  /* next element in the ring */
  char *buffer_base;      /* in case we have to free it at somepoint */
  char *buffer_ptr;       /* the aligned and offset pointer */
};

typedef struct ring_elt *ring_elt_ptr;

enum {
  SEND_CALLS = 0,
  BYTES_SENT,
  RECV_CALLS,
  BYTES_RECEIVED,
  TRANS_SENT,
  TRANS_RECEIVED,
  CONNECT_CALLS,
  ACCEPT_CALLS,
  RETRANSMITS,
  BSD_MAX_COUNTERS
};

/* PN: 07/31/2007
 * Test specific data for helper (child) threads. I.e., tests such as
 * recv_tcp_rr_multi test will support concurrent send_tcp_rr
 * tests. After accept()ing each sender, a new helper thread is spawned
 * to handle the sender. This helper thread is launched with the following
 * data.
 */
typedef struct generic_helper_data {

  NETPERF_THREAD_T tid;        /* Thread id */

  SOCKET           s_data;     /* data connection to a sender */

  struct ring_elt *send_ring;  /* address of the send_ring */
  struct ring_elt *recv_ring;  /* address of the recv_ring */

  uint32_t         state_req;  /* the state to which this helper
                                * has been requested to transition.
                                * This field is monitored by the helper
                                * thread and takes action accordingly.
                                * The recv_tcp_rr_multi test thread
                                * requests for state changes. */

  uint32_t   new_state;        /* the state the helper is currently in.
                                * this field is modified by the helper when
                                * it has transitioned to a new state. */

  uint64_t  stats_counter;     /* Whatever stats that should be collected
                                * For recv_tcp_rr_multi_helper_data, this will
                                * be number of trans received. */

  test_t   *test_being_helped; /* Pointer to the test data who is being helped
                                * The helping threads will read test specific
                                * parameters using this pointer. The helper
                                * threads cannot (rather should not, due to
                                * possible race conditions) write anything
                                * to this test data.
                                */

} generic_helper_data_t;

typedef struct  bsd_test_data {
  /* address information */
  struct addrinfo *locaddr;        /* local address informtion */
  struct addrinfo *remaddr;        /* remote address informtion */

  SOCKET           s_listen;       /* listen sockets for catching type tests */
  SOCKET           s_data;         /* data socket for executing tests */

  struct ring_elt *send_ring;      /* address of the send_ring */
  struct ring_elt *recv_ring;      /* address of the recv_ring */
  FILE            *fill_source;    /* pointer to file for filling rings */

  /* send parameters */
  int   sbuf_size_ret;  /* send socket buffer size returned on creation */
  int   send_buf_size;  /* send socket buffer size */
  int   send_size;      /* how many bytes to send at one time? */
  int   send_avoid;     /* do we want to avoid send copies? */
  int   send_align;     /* what is the alignment of the send buffer? */
  int   send_offset;    /* and at what offset from that alignment? */
  int   no_delay;       /* do we disable the nagle algorithm for send */
  int   burst_count;    /* number of sends in an interval or first burst */
  int   interval;       /* minimum time from start to start of bursts */

  /* recv parameters */
  int   rbuf_size_ret;  /* receive socket buffer size returned on creation */
  int   recv_buf_size;  /* receive socket buffer size */
  int   recv_size;      /* how many bytes do receive at one time? */
  int   recv_avoid;     /* do we want to avoid copies on receives? */
  int   recv_align;     /* what is the alignment of the receive buffer? */
  int   recv_offset;    /* and at what offset from that alignment? */
  int   recv_dirty_cnt; /* how many integers in the receive buffer */
                        /* should be made dirty before calling recv? */
  int   recv_clean_cnt; /* how many integers should be read from the */
                        /* recv buffer before calling recv? */

  /* connection parameters */
  /* request/response parameters */
  int  request_size;    /* number of bytes to send */
  int  response_size;   /* number of bytes to receive */

  /* other parameters */
  int   port_min;
  int   port_max;
  int   send_width;
  int   recv_width;
  int   req_size;
  int   rsp_size;
  int   multi_queue;

  /* PN: 07/31/2007
   * A test such as recv_tcp_rr_multi spawns helper threads to
   * handle concurrent senders. This is an array
   * (one for each helper thread) of helper data.
   */  
  generic_helper_data_t *helper_data;

  /* PN: 07/31/2007
   * Number of concurrent connections/sockets to handle.
   * (that many helper threads will be spawned
   */
  int   num_concurrent_conns;


  /* data structures for UDP RR packet loss detection and retransmission. */
  int    retry_index;
  int    pending_responses;
  char **retry_array;  /* addresses of entries in the send_ring */

  /* Statistics Counters */
  union {
    uint64_t  counter[BSD_MAX_COUNTERS];
    struct {
      uint64_t  send_calls;
      uint64_t  bytes_sent;
      uint64_t  recv_calls;
      uint64_t  bytes_received;
      uint64_t  trans_sent;
      uint64_t  trans_received;
      uint64_t  connect_calls;
      uint64_t  accepts;
      uint64_t  retransmits;
    } named;
  } stats;
  struct timeval  elapsed_time;
  struct timeval  prev_time;
  struct timeval  curr_time;

  /* Place for HISTOGRAM fields */
  HIST time_hist;
} bsd_data_t;

/* PN: Each stat test gets its
 * own CSV file */
typedef struct csv_file_pointers {
    FILE *test_stats;
    FILE *sys_stats;
    FILE *tcp_stats;
    FILE *netdev_stats;
} csv_files_t;

typedef struct  bsd_results_data {
  int     max_count;
  int     print_test;
  int     print_run;
  int     print_per_cpu;
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
  double result_interval;
  double result_confidence;
  double result_minimum;
  double result_maximum;
  double cpu_util_measured_mean;
  double cpu_util_interval;
  double cpu_util_confidence;
  double service_demand_measured_mean;
  double service_demand_interval;
  double service_demand_confidence;
  double confidence;
  double sd_denominator;

  /* PN: CSV output */
  csv_files_t csv_files;
  int    print_csv;  /* Flag to enable CSV output. User can
                      * turn this on using report_flags attribute
                      * (value = "PRINT_CSV") of report_stats.
                      * Refer netperf_docs.dtd.
                      */
  time_t  timestamp; /* To group related test runs across CSV files */

  double results_start;  /* must be the last field in structure */
} bsd_results_t;

/* Error codes to be used within nettest_bsd */
enum {
  BSDE_MAX_ERROR = -32,
  BSDE_SOCKET_SHUTDOWN_FAILED,
  BSDE_BIND_FAILED,
  BSDE_GETADDRINFO_ERROR,
  BSDE_XMLSETPROP_ERROR,
  BSDE_XMLNEWNODE_ERROR,
  BSDE_NO_SOCKET_ARGS,
  BSDE_SOCKET_ERROR,
  BSDE_SETSOCKOPT_ERROR,
  BSDE_LISTEN_FAILED,
  BSDE_GETSOCKNAME_FAILED,
  BSDE_REQUESTED_STATE_INVALID,
  BSDE_ACCEPT_FAILED,
  BSDE_DATA_RECV_ERROR,
  BSDE_TEST_STATE_CORRUPTED,
  BSDE_CONNECT_FAILED,
  BSDE_DATA_CONNECTION_CLOSED_ERROR,
  /* PN: CSV output */
  BSDE_CSV_FILE_OPEN_ERROR,
  BSDE_DATA_SEND_ERROR=-1,
  BSDE_SUCCESS = 0
};
