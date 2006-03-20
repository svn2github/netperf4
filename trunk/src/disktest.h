


/* This file is Hewlett_Packard Company Confidental */

 /* This file contains the test-specific definitions for netperf4's */
 /* Hewlett-Packard Company disk tests */

enum {
  READ_CALLS = 0,
  BYTES_READ,
  WRITE_CALLS,
  BYTES_WRITTEN,
  LSEEK_CALLS,
  DISK_MAX_COUNTERS
};

typedef struct  disk_test_data {
  /* parameters */
  int                  fd;
  size_t               chunk;
  float                read;
  char                *file_name;
  char                *rpath;
  char                *bpath;
  char                *mount_point;
  off_t                where;
  off_t                testSize;
  off_t                start_pos;
  off_t                end_pos;
  int                  scsi_immreport;
  int                  scsi_queue_depth;

  void                *orig_buffer;
  void                *buffer_start;

  /* information about the disk */
  off_t                    devSize;
  off_t                    diskSize;
  off_t                    sectSize;
  int                      immReportValue;
  int                      OrigImmReportValue;
  uint32_t                 queueDepthValue;
  uint32_t                 OrigQueueDepthValue;
  disk_describe_type       dev_info;
  union  inquiry_data      scsi_inq;
  struct sioc_lun_limits   lun_limits;
  
  /* Statistics Counters */
  union {
    uint64_t  counter[DISK_MAX_COUNTERS];
    struct {
      uint64_t  read_calls;
      uint64_t  bytes_read;
      uint64_t  write_calls;
      uint64_t  bytes_written;
      uint64_t  lseek_calls;
    } named;
  } stats;
  struct timeval  elapsed_time;
  struct timeval  prev_time;
  struct timeval  curr_time;

  /* Place for HISTOGRAM fields */
  HIST read_hist;
  HIST write_hist;
} disk_data_t;

typedef struct  disk_results_data {
  int     max_count;
  int     print_hist;
  int     print_per_cpu;
  int     print_test;
  int     print_run;
  FILE   *outfd;
  double *iops;
  double *read_iops;
  double *write_iops;
  double *read_results;
  double *write_results;
  double *utilization;
  double *servdemand;
  double *run_time;
  double ave_time;
  double iops_measured_mean;
  double iops_interval;
  double iops_confidence;
  double iops_minimum;
  double iops_maximum;
  double read_measured_mean;
  double read_interval;
  double read_confidence;
  double write_measured_mean;
  double write_interval;
  double write_confidence;
  double cpu_util_measured_mean;
  double cpu_util_interval;
  double cpu_util_confidence;
  double service_demand_measured_mean;
  double service_demand_interval;
  double service_demand_confidence;
  double confidence;
  double sd_denominator;
  double results_start;  /* must be the last field in structure */
} disk_results_t;

/* Error codes to be used within disktest */
enum {
  DISK_MAX_ERROR = -32,
  DISK_XMLSETPROP_ERROR,
  DISK_XMLNEWNODE_ERROR,
  DISK_SIZE_TO_LARGE,
  DISK_START_POS_TO_LARGE,
  DISK_END_POS_TO_LARGE,
  DISK_TEST_STAT_FAILED,
  DISK_TEST_OPEN_FAILED,
  DISK_TEST_RDWR_OPEN_FAILED,
  DISK_TEST_DESCRIBE_FAILED,
  DISK_TEST_DIOC_CAPACITY_FAILED,
  DISK_TEST_SIOC_INQUIRY_FAILED,
  DISK_TEST_SIOC_GET_IR_FAILED,
  DISK_TEST_NOT_IO_DEVICE,
  DISK_TEST_NOT_CHARACTER_FILE,
  DISK_TEST_ERROR_WRITING_FILE,
  DISK_TEST_CANT_WRITE_FILE,
  DISK_TEST_ERROR_PARTIAL_READ,
  DISK_TEST_ERROR_READING_FILE,
  DISK_TEST_SEEK_ERROR,
  DISK_TEST_EOF_ERROR,
  DISK_NO_DISK_ARGS,
  DISK_MALLOC_FAILED,
  DISK_TEST_STATE_CORRUPTED,
  DISK_REQUESTED_STATE_INVALID,
  DISK_SUCCESS = 0
};
