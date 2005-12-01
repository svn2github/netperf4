
/* This file is Hewlett_Packard Company Confidental */

 /* This file contains the test-specific definitions for netperf4's */
 /* Hewlett-Packard Company special variable sized data tests */

#define MAX_PATTERNS 16
#define VST_RAND_STATE_SIZE 128

typedef struct vst_ring_elt *vst_ring_ptr;

typedef struct distribution_data {
  int           num_patterns;
  int           dist_key;
  int           dist_count[MAX_PATTERNS];
  vst_ring_ptr  pattern[MAX_PATTERNS];
  char          random_state[VST_RAND_STATE_SIZE];
} dist_t;

typedef struct vst_ring_elt {
  vst_ring_ptr     next;           /* next element in the ring */
  char            *recv_buff_base; /* in case we have to free at somepoint */
  char            *recv_buff_ptr;  /* the aligned and offset pointer */
  int              recv_size;      /* size of request to place in the buffer */
  int              recv_buff_size; /* the actual size of the physical buffer */
  char            *send_buff_base; /* in case we have to free at somepoint */
  char            *send_buff_ptr;  /* the aligned and offset pointer */
  int              send_size;      /* size of response to place in the buffer */
  int              send_buff_size; /* the actual size of the physical buffer */
  dist_t          *distribution;   /* pointer to a distribution structure */
} vst_ring_elt_t;


enum {
  SEND_CALLS = 0,
  BYTES_SENT,
  RECV_CALLS,
  BYTES_RECEIVED,
  TRANS_SENT,
  TRANS_RECEIVED,
  CONNECT_CALLS,
  ACCEPT_CALLS,
  VST_MAX_COUNTERS
};

typedef struct  vst_test_data {
  /* address information */
  struct addrinfo *locaddr;        /* local address informtion */
  struct addrinfo *remaddr;        /* remote address informtion */

  int              s_listen;       /* listen sockets for catching type tests */
  int              s_data;         /* data socket for executing tests */
  vst_ring_ptr     vst_ring;       /* address of the vst_ring */
  FILE            *fill_source;    /* pointer to file for filling rings */
  xmlNodePtr       wld;            /* pointer to the work load description
                                      for this test */
  /* send parameters */
  int   sbuf_size_ret;  /* send socket buffer size returned on creation */
  int   send_buf_size;  /* send socket buffer size */
  int   send_size;      /* how many bytes maximum to send at one time? */
  int   send_avoid;     /* do we want to avoid send copies? */
  int   send_align;     /* what is the alignment of the send buffer? */
  int   send_offset;    /* and at what offset from that alignment? */
  int   no_delay;       /* do we disable the nagle algorithm for send */
  

  /* recv parameters */
  int   rbuf_size_ret;  /* receive socket buffer size returned on creation */
  int   recv_buf_size;  /* receive socket buffer size */
  int   recv_size;      /* how many bytes maximum to receive at one time? */
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

  /* Statistics Counters */
  union {
    uint64_t  counter[VST_MAX_COUNTERS];
    struct {
      uint64_t  send_calls;
      uint64_t  bytes_sent;
      uint64_t  recv_calls;
      uint64_t  bytes_received;
      uint64_t  trans_sent;
      uint64_t  trans_received;
      uint64_t  connect_calls;
      uint64_t  accepts;
    } named;
  } stats;
  struct timeval  elapsed_time;
  struct timeval  prev_time;
  struct timeval  curr_time;

  /* Place for HISTOGRAM fields */
  HIST time_hist;
} vst_data_t;

typedef struct  vst_results_data {
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
  double xmit_measured_mean;
  double recv_measured_mean;
  double cpu_util_measured_mean;
  double cpu_util_confidence;
  double service_demand_measured_mean;
  double service_demand_confidence;
  double confidence;
  double sd_denominator;
  double results_start;  /* must be the last field in structure */
} vst_results_t;

/* Error codes to be used within nettest_vst */
enum {
  VSTE_MAX_ERROR = -32,
  VSTE_SOCKET_SHUTDOWN_FAILED,
  VSTE_BIND_FAILED,
  VSTE_GETADDRINFO_ERROR,
  VSTE_XMLSETPROP_ERROR,
  VSTE_XMLNEWNODE_ERROR,
  VSTE_NO_SOCKET_ARGS,
  VSTE_SOCKET_ERROR,
  VSTE_SETSOCKOPT_ERROR,
  VSTE_LISTEN_FAILED,
  VSTE_GETSOCKNAME_FAILED,
  VSTE_REQUESTED_STATE_INVALID,
  VSTE_ACCEPT_FAILED,
  VSTE_DATA_RECV_ERROR,
  VSTE_TEST_STATE_CORRUPTED,
  VSTE_CONNECT_FAILED,
  VSTE_DATA_SEND_ERROR,
  VSTE_DATA_CONNECTION_CLOSED_ERROR,
  VSTE_SUCCESS = 0
};
