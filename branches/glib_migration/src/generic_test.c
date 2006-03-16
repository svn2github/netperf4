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

char    generic_test_id[]="\
@(#)generic_test.c (c) Copyright 2005 Hewlett-Packard Co. $Id$";

/* Macros for accessing fields in the global netperf structures. */
#define CHECKREQ_STATE                   test->state_req
#define GET_TEST_DATA(test)              test->test_specific_data
#define GET_TEST_STATE                   test->new_state
#define SET_TEST_DATA(ptr)               test->test_specific_data = ptr
#define SET_TEST_STATE(state)            test->new_state = state

static void
report_test_failure(test, function, err_code, err_string)
  test_t *test;
  char   *function;
  int     err_code;
  char   *err_string;
{
  if (test->debug) {
    fprintf(test->where,"%s: called report_test_failure:",function);
    fprintf(test->where,"reporting  %s  errno = %d\n",err_string,GET_ERRNO);
    fflush(test->where);
  }
  test->err_rc    = err_code;
  test->err_fn    = function;
  test->err_str   = err_string;
  test->new_state = TEST_ERROR;
  test->err_no    = GET_ERRNO;
}

static void
set_test_state(test_t *test, uint32_t new_state)
{
  int  curr_state;
  int  state;
  int  valid = 0;
  char *state_name;
  char error_msg[1024];

  curr_state = GET_TEST_STATE;
  if (curr_state != TEST_ERROR) {
    if (curr_state != new_state) {
      switch (curr_state) {
      case TEST_PREINIT:
        state = TEST_INIT;
        valid = 1;
        break;
      case TEST_INIT:
        state_name = "TEST_INIT";
        if (new_state == TEST_IDLE) {
          state = TEST_IDLE;
          valid = 1;
        }
        break;
      case TEST_IDLE:
        state_name = "TEST_IDLE";
        if (new_state == TEST_LOADED) {
          state = TEST_LOADED;
          valid = 1;
        }
        if (new_state == TEST_DEAD) {
          state = TEST_DEAD;
          valid = 1;
        }
        break;
      case TEST_LOADED:
        state_name = "TEST_LOADED";
        if (new_state == TEST_MEASURE) {
          state = TEST_MEASURE;
          valid = 1;
        }
        if (new_state == TEST_IDLE) {
          state = TEST_IDLE;
          valid = 1;
        }
        break;
      case TEST_MEASURE:
        state_name = "TEST_MEASURE";
        if (new_state == TEST_LOADED) {
          state = TEST_LOADED;
          valid = 1;
        }
        break;
      case TEST_ERROR:
        /* an error occured while processing in the current state
           return and we should drop into wait_to_die so that
           netperf4 can retrieve the error information before killing
           the test */
        state_name = "TEST_ERROR";
        break;
      default:
        state_name = "ILLEGAL";
      }
      if (valid) {
        SET_TEST_STATE(state);
      } else {
        sprintf(error_msg,"bad state transition from %s state",state_name);
        report_test_failure( test,
                             "set_test_state",
                             BSDE_REQUESTED_STATE_INVALID,
                             strdup(error_msg));
      }
    }
  }
}


void
wait_to_die(test_t *test)
{
  while (GET_TEST_STATE != TEST_DEAD) {
    if (CHECK_REQ_STATE == TEST_DEAD) {
      free(GET_TEST_DATA(test));
      SET_TEST_DATA(NULL);
      SET_TEST_STATE(TEST_DEAD);
    }
  }
}

/* the following lines are a template for any test
   just copy the lines for generic_test change
   the procedure name and write your own TEST_SPECIFC_XXX
   functions.  Have Fun   sgb 2005-10-26 */

void
generic_test(test_t *test)
{
  uint32_t state, new_state;
  TEST_SPECIFIC_INITIALIZE(test);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      TEST_SPECIFIC_PREINIT(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = TEST_SPECIFIC_INIT(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        sleep(1);
      }
      break;
    case TEST_MEASURE:
      new_state = TEST_SPECIFIC_MEASURE(test);
      break;
    case TEST_LOAD:
      new_state = TEST_SPECIFIC_LOAD(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
}  /* end of generic_test */
 
/* end of generic_test example code  sgb  2005-10-26 */

