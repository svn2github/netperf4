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
  uint64_t other;     /* the number of time units in other states
                         and/or any slop we have */
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


