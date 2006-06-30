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

#ifndef lint
char    disk_test_id[]="\
@(#)disktest.c (c) Copyright 2005 Hewlett-Packard Co. $Id: disktest.c 166 2006-04-19 00:09:40Z raj $";
#endif /* lint */


/****************************************************************/
/*                                                              */
/*      disk.c                                                  */
/*                                                              */
/*      the actual test routines...                             */
/*                                                              */
/*                                                              */
/****************************************************************/



/* requires 64 bit file offsets */
#define _FILE_OFFSET_BITS 64

/* turn on histogram capability */
#define WANT_HISTOGRAM


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef OFF
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIM
#include <time.h>
#endif
#endif
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/diskio.h>
#include <sys/scsi.h>

#include "netperf.h"

#include "disktest.h"


static void
report_test_failure(test, function, err_code, err_string)
  test_t *test;
  char   *function;
  int     err_code;
  char   *err_string;
{
  int loc_debug = 1;
  if (test->debug || loc_debug) {
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
      } 
      else {
        sprintf(error_msg,"bad state transition from %s state",state_name);
        report_test_failure( test,
                             (char *)__func__,
                             DISK_REQUESTED_STATE_INVALID,
                             strdup(error_msg));
      }
    }
  }
}


void
wait_to_die(test_t *test, void(*free_routine)(test_t *))
{
  while (GET_TEST_STATE != TEST_DEAD) {
    if (CHECK_REQ_STATE == TEST_DEAD) {
  /* add extra ioctl to restore original scsi queue depth in the future */
  /* add extra ioctl to restore original scsi immreport in the future */
      free_routine(test);
      free(GET_TEST_DATA(test));
      SET_TEST_DATA(test,NULL);
      SET_TEST_STATE(TEST_DEAD);
    }
  }
}

static ssize_t
io_read(int fd, void *buf_addr, size_t size, test_t *test, char *func)
{
  ssize_t bytes;
  

  if ((bytes = read(fd, buf_addr, size)) != size) {
    if (bytes == 0L) {
      report_test_failure(test, func,
                          DISK_TEST_EOF_ERROR,
                          "attempted to read past end of file");
    }
    else if (bytes == -1L) {
      report_test_failure(test, func,
                          DISK_TEST_ERROR_READING_FILE,
                          "error reading file");
    }
    else {
      report_test_failure(test, func,
                          DISK_TEST_ERROR_PARTIAL_READ,
                          "partial read from file");
    }
  }
  return(bytes);
}


static ssize_t
io_write(int fd, void *buf_addr, size_t size, test_t *test, char *func)
{
  ssize_t bytes;
  

  if ((bytes = write(fd, buf_addr, size)) != size) {
    if (bytes < 0L) {
      report_test_failure(test, func,
                          DISK_TEST_CANT_WRITE_FILE,
                          "can not write to file");
    }
    else {
      report_test_failure(test, func,
                          DISK_TEST_ERROR_WRITING_FILE,
                          "error while writing to file");
    }
  }
  return(bytes);
}



static void
free_disk_test_data(test_t *test)
{
  disk_data_t *my_data;
  
  my_data  = GET_TEST_DATA(test);

  free(my_data->orig_buffer);
  close(my_data->fd);

  my_data->orig_buffer   = NULL;
  my_data->buffer_start  = NULL;
}


static void
disk_test_init(test_t *test)
{
  disk_data_t *new_data;
  xmlNodePtr   args;
  xmlChar     *string;
  int          loc_debug = 0;
  

  new_data = (disk_data_t *)malloc(sizeof(disk_data_t));
  args = test->node->xmlChildrenNode;
  while (args != NULL) {
    if (xmlStrcmp(args->name,(const xmlChar *)"disk_args")) {
      args = args->next;
      continue;
    }
    break;
  }

  if ((args != NULL) &&
      (NULL != new_data)) {
    memset(new_data,0,sizeof(disk_data_t));

    string =  xmlGetProp(args,(const xmlChar *)"file_name");
    new_data->file_name = (char *)string;
    string =  xmlGetProp(args,(const xmlChar *)"read");
    if (string) {
      new_data->read = atof((char *)string);
    } else {
      new_data->read = 1.0;
    }

    string =  xmlGetProp(args,(const xmlChar *)"disk_io_size");
    if (test->debug || loc_debug) {
      fprintf(test->where,
              "%s:%s  disk_io_size = '%s'\n", test->id, __func__, string);
      fflush(test->where);
    }
    if (string) {
      new_data->chunk = strtoul((char *)string,NULL,10);
    } else {
      new_data->chunk = 8;
    }
    string =  xmlGetProp(args,(const xmlChar *)"disk_io_units");
    if (test->debug || loc_debug) {
      fprintf(test->where,
              "%s:%s  disk_io_units = '%s'\n", test->id, __func__, string);
      fflush(test->where);
    }
    if (string) {
      if (strstr((char *)string,"KB")) {
        new_data->chunk *= 1024;
      }
      if (strstr((char *)string,"MB")) {
        new_data->chunk *= (1024 * 1024);
      }
      if (strstr((char *)string,"GB")) {
        new_data->chunk *= (1024 * 1024 * 1024);
      }
    }
    else {
      new_data->chunk *= 1024;
    }

    string =  xmlGetProp(args,(const xmlChar *)"disk_test_size");
    if (test->debug || loc_debug) {
      fprintf(test->where,
              "%s:%s  disk_test_size = '%s'\n", test->id, __func__, string);
      fflush(test->where);
    }
    if (string) {
      new_data->testSize = strtoull((char *)string,NULL,10);
    } else {
      new_data->testSize = 0;
    }
    string =  xmlGetProp(args,(const xmlChar *)"disk_test_units");
    if (test->debug || loc_debug) {
      fprintf(test->where,
              "%s:%s  disk_test_units = '%s'\n", test->id, __func__, string);
      fflush(test->where);
    }
    if (string) {
      if (strstr((char *)string,"KB")) {
        new_data->testSize *= 1024;
      }
      if (strstr((char *)string,"MB")) {
        new_data->testSize *= (1024 * 1024);
      }
      if (strstr((char *)string,"GB")) {
        new_data->testSize *= (1024 * 1024 * 1024);
      }
    }
    else {
      new_data->testSize *= 1024;
    }

    string =  xmlGetProp(args,(const xmlChar *)"start_position");
    if (string) {
      new_data->start_pos = strtoull((char *)string,NULL,10);
      new_data->start_pos *= 1024;
    } else {
      new_data->start_pos = 0;
    }
    string =  xmlGetProp(args,(const xmlChar *)"end_position");
    if (string) {
      new_data->end_pos = strtoull((char *)string,NULL,10);
      new_data->end_pos *= 1024;
    } else {
      new_data->end_pos = 0;
    }
    string =  xmlGetProp(args,(const xmlChar *)"scsi_immreport");
    if (string) {
      new_data->scsi_immreport = atoi((char *)string);
    } else {
      new_data->scsi_immreport = -1;
    }
    string =  xmlGetProp(args,(const xmlChar *)"scsi_queue_depth");
    if (string) {
      new_data->scsi_queue_depth = atoi((char *)string);
    } else {
      new_data->scsi_queue_depth = -1;
    }
  }
  if (args == NULL) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_NO_DISK_ARGS,
                        "no disk_arg element was found");
  }
  if (new_data == NULL) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_MALLOC_FAILED,
                        "malloc failed in disk_test_init");
  }
  if (test->debug || loc_debug) {
    fprintf(test->where,
            "%s:%s  file_name = '%s'\n", test->id, __func__, 
            new_data->file_name);
    fprintf(test->where,
            "%s:%s  read = %f\n", test->id, __func__, 
            new_data->read);
    fprintf(test->where,
            "%s:%s  chunk = %ld\n", test->id, __func__, 
            new_data->chunk);
    fprintf(test->where,
            "%s:%s  testSize = %lld\n", test->id, __func__, 
            new_data->testSize);
    fprintf(test->where,
            "%s:%s  scsi_immreport = %d\n", test->id, __func__, 
            new_data->scsi_immreport);
    fprintf(test->where,
            "%s:%s  scsi_queue_depth = %d\n", test->id, __func__, 
            new_data->scsi_queue_depth);
    fprintf(test->where, "\n");
    fflush(test->where);
  }
  
  new_data->read_hist  = HIST_new();
  new_data->write_hist = HIST_new();

  SET_TEST_DATA(test, new_data);
}


static void
update_elapsed_time(disk_data_t *my_data)
{
  my_data->elapsed_time.tv_usec += my_data->curr_time.tv_usec;
  my_data->elapsed_time.tv_usec -= my_data->prev_time.tv_usec;

  my_data->elapsed_time.tv_sec += my_data->curr_time.tv_sec;
  my_data->elapsed_time.tv_sec -= my_data->prev_time.tv_sec;

  if (my_data->curr_time.tv_usec < my_data->prev_time.tv_usec) {
    my_data->elapsed_time.tv_usec += 1000000;
    my_data->elapsed_time.tv_sec--;
  }

  if (my_data->elapsed_time.tv_usec >= 1000000) {
    my_data->elapsed_time.tv_usec -= 1000000;
    my_data->elapsed_time.tv_sec++;
  }
}


static int
disk_test_clear_stats(disk_data_t *my_data)
{
  int i;
  for (i = 0; i < DISK_MAX_COUNTERS; i++) {
    my_data->stats.counter[i] = 0;
  }
  HIST_CLEAR(my_data->read_hist);
  HIST_CLEAR(my_data->write_hist);
  my_data->elapsed_time.tv_usec = 0;
  my_data->elapsed_time.tv_sec  = 0;
  gettimeofday(&(my_data->prev_time),NULL);
  my_data->curr_time = my_data->prev_time;
  return(NPE_SUCCESS);
}


static void
disk_test_decode_stats(xmlNodePtr stats,test_t *test)
{
  if (test->debug) {
    fprintf(test->where,"%s: entered for %s test %s\n",
            __func__, test->id, test->test_name);
    fflush(test->where);
  }
}


static xmlNodePtr
disk_test_get_stats(test_t *test)
{
  xmlNodePtr  stats = NULL;
  xmlNodePtr  hist  = NULL;
  xmlAttrPtr  ap    = NULL;
  int         i,j;
  char        value[64];
  char        name[64];
  uint64_t    loc_cnt[DISK_MAX_COUNTERS];

  disk_data_t *my_data = GET_TEST_DATA(test);

  if (test->debug) {
    fprintf(test->where,"%s: entered for %s test %s\n",
            __func__, test->id, test->test_name);
    fflush(test->where);
  }
  if ((stats = xmlNewNode(NULL,(xmlChar *)"test_stats")) != NULL) {
    /* set the properites of the test_stats message -
       the tid and time stamps/values and counter values  sgb 2006-03-15 */

    ap = xmlSetProp(stats,(xmlChar *)"tid",test->id);
    for (i = 0; i < DISK_MAX_COUNTERS; i++) {
      loc_cnt[i] = my_data->stats.counter[i];
      if (test->debug) {
        fprintf(test->where,"DISK_COUNTER%X = %#llx\n",i,loc_cnt[i]);
      }
    }
    if (GET_TEST_STATE == TEST_MEASURE) {
      gettimeofday(&(my_data->curr_time), NULL);
      if (ap != NULL) {
        sprintf(value,"%ld",my_data->curr_time.tv_sec);
        ap = xmlSetProp(stats,(xmlChar *)"time_sec",(xmlChar *)value);
        if (test->debug) {
          fprintf(test->where,"time_sec=%s\n",value);
          fflush(test->where);
        }
      }
      if (ap != NULL) {
        sprintf(value,"%ld",my_data->curr_time.tv_usec);
        ap = xmlSetProp(stats,(xmlChar *)"time_usec",(xmlChar *)value);
        if (test->debug) {
          fprintf(test->where,"time_usec=%s\n",value);
          fflush(test->where);
        }
      }
    }
    else {
      if (ap != NULL) {
        sprintf(value,"%ld",my_data->elapsed_time.tv_sec);
        ap = xmlSetProp(stats,(xmlChar *)"elapsed_sec",(xmlChar *)value);
        if (test->debug) {
          fprintf(test->where,"elapsed_sec=%s\n",value);
          fflush(test->where);
        }
      }
      if (ap != NULL) {
        sprintf(value,"%ld",my_data->elapsed_time.tv_usec);
        ap = xmlSetProp(stats,(xmlChar *)"elapsed_usec",(xmlChar *)value);
        if (test->debug) {
          fprintf(test->where,"elapsed_usec=%s\n",value);
          fflush(test->where);
        }
      }
    }
    for (i = 0; i < DISK_MAX_COUNTERS; i++) {
      if (ap == NULL) {
        break;
      }
      if (loc_cnt[i]) {
        sprintf(value,"%#llx",my_data->stats.counter[i]);
        sprintf(name,"cntr%1X_value",i);
        ap = xmlSetProp(stats,(xmlChar *)name,(xmlChar *)value);
        if (test->debug) {
          fprintf(test->where,"%s=%s\n",name,value);
          fflush(test->where);
        }
      }
    }
    /* add hist_stats entries to the status report */
    if (my_data->stats.named.read_calls > 0) {
      snprintf(name,32,"DISK_READ test %s",test->id);
      hist = HIST_stats_node(my_data->read_hist, name);
      if (hist != NULL) {
        xmlAddChild(stats,hist);
      }
    }
    if (my_data->stats.named.write_calls > 0) {
      snprintf(name,63,"DISK_WRITE test %s",test->id);
      hist = HIST_stats_node(my_data->write_hist, name);
      if (hist != NULL) {
        xmlAddChild(stats,hist);
      }
    }
    if (ap == NULL) {
      xmlFreeNode(stats);
      stats = NULL;
    }
  }
  if (test->debug) {
    fprintf(test->where,"%s: exiting for %s test %s\n",
            __func__, test->id, test->test_name);
    fflush(test->where);
  }
  return(stats);
} /* end of disk_test_get_stats */



static void
fix_up_parms(test_t *test)
{
  disk_data_t  *my_data;
  off_t         max_chunk;
  off_t         value;
  char         *buf;
  int           loc_debug = 0;


  my_data     = GET_TEST_DATA(test);

  /* default testSize */
  if (my_data->testSize == 0) {
    if (my_data->start_pos < (my_data->devSize - (my_data->chunk * 2))) {
      my_data->testSize = my_data->devSize - my_data->start_pos;
    }
    else {
      my_data->testSize = my_data->devSize;
    }
    value = my_data->chunk * 512;
    if (my_data->testSize > value) {
      my_data->testSize = value;
    }
  }

  /* default chunk if 8K is to large */
  max_chunk = my_data->testSize / 2;
  if (my_data->chunk > max_chunk) {
    my_data->chunk = max_chunk;
  }

  /* default end_position */
  if (my_data->end_pos == 0) {
    if (my_data->start_pos < (my_data->devSize - (my_data->chunk * 2))) {
      my_data->end_pos = my_data->start_pos + my_data->testSize;
    }
    else {
      my_data->end_pos = my_data->testSize;
    }
  }
  
  if (my_data->testSize > my_data->devSize) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_TEST_SIZE_TO_LARGE,
                        "disk test size is larger than device capacity");
  }
  else if (my_data->start_pos > (my_data->devSize - my_data->testSize)) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_START_POS_TO_LARGE,
                        "start_position value is to large");
  }
  else if (my_data->end_pos > my_data->devSize) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_END_POS_TO_LARGE,
                        "end_postion value is past end of device");
  }
  else if (my_data->end_pos > (my_data->start_pos + my_data->testSize)) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_END_POS_TO_LARGE,
                        "end_postion value is past end of testSize");
  }
  else if (my_data->chunk > (my_data->testSize/2)) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_IO_SIZE_TO_LARGE,
                        "disk io size is too large");
  }
  my_data->where = my_data->start_pos;
  
  value = sysconf(_SC_PAGE_SIZE);
  buf = malloc(my_data->chunk + value - 1);
  my_data->orig_buffer = buf;
  buf = (char *)(((long)buf + (long)value - 1) & ~((long)value -1));
  my_data->buffer_start = buf;

  if (test->debug || loc_debug) {
    fprintf(test->where,
            "%s:%s  testSize = %lld\n", test->id, __func__, 
            my_data->testSize);
    fprintf(test->where,
            "%s:%s  chunk = %ld\n", test->id, __func__, 
            my_data->chunk);
    fprintf(test->where,
            "%s:%s  start_position = %lld\n", test->id, __func__, 
            my_data->start_pos);
    fprintf(test->where,
            "%s:%s  end_position = %lld\n", test->id, __func__, 
            my_data->end_pos);
    fprintf(test->where,
            "%s:%s  devSize = %lld\n", test->id, __func__, 
            my_data->devSize);
    fprintf(test->where,
            "%s:%s  diskSize = %lld\n", test->id, __func__, 
            my_data->diskSize);
    fprintf(test->where,
            "%s:%s  sectSize = %lld\n", test->id, __func__, 
            my_data->sectSize);
    fprintf(test->where,
            "%s:%s  where = %lld\n", test->id, __func__, 
            my_data->where);
    fprintf(test->where,
            "%s:%s  buffer_start = %p\n", test->id, __func__, 
            my_data->buffer_start);
    fflush(test->where);
  }
}



static void
raw_disk_init(test_t *test)
{
  int                       fd = -1;
  disk_describe_type       *describe;
  union inquiry_data       *scsi_inq;
  struct sioc_lun_limits   *lun_limits;
  disk_data_t              *my_data;
  struct stat               stat_buf;
  struct capacity           scsi_capacity;
  capacity_type             capacity;
  int                       ir_flag;
  int                       loc_debug = 0;


  my_data     = GET_TEST_DATA(test);
  describe    = &(my_data->dev_info);
  scsi_inq    = &(my_data->scsi_inq);
  lun_limits  = &(my_data->lun_limits);

  if (stat(my_data->file_name, &stat_buf) < 0) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_TEST_STAT_FAILED,
                        "stat of file_name failed");
  }
  else if ((stat_buf.st_mode & S_IFMT) != S_IFCHR) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_TEST_NOT_CHARACTER_FILE,
                        "Test needs a character device");
  }
  else if ((fd = open(my_data->file_name, O_RDONLY)) < 0) {
    fprintf(test->where,
            "%s: open of file %s failed\n", __func__, my_data->file_name);
    fflush(test->where);
    report_test_failure(test,
                        (char *)__func__,
                        DISK_TEST_OPEN_FAILED,
                        "open of file_name failed");
  }
  /* get disk information */
  else if (ioctl(fd, DIOC_DESCRIBE, describe) < 0) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_TEST_DESCRIBE_FAILED,
                        "ioctl to get disk description failed");
  }
  else if ((describe->dev_type  == UNKNOWN_DEV_TYPE) &&
           (describe->intf_type == UNKNOWN_INTF)) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_TEST_NOT_IO_DEVICE,
                        "Test needs an I/O Device");
  }
  else {
    my_data->sectSize = describe->lgblksz;
    my_data->diskSize = (off_t)(describe->maxsva + 1) * my_data->sectSize;

    switch(describe->intf_type) {
    /* SCSI drives */
    case SCSI_INTF:
      if (ioctl(fd, SIOC_INQUIRY, &scsi_inq) < 0) {
        report_test_failure(test,
                            (char *)__func__,
                            DISK_TEST_SIOC_INQUIRY_FAILED,
                            "ioctl to get scsi disk information failed");
      }
      if (ioctl(fd, SIOC_CAPACITY, &scsi_capacity) < 0) {
        if (ioctl(fd, DIOC_CAPACITY, &capacity) < 0) {
          report_test_failure(test,
                              (char *)__func__,
                              DISK_TEST_DIOC_CAPACITY_FAILED,
                              "ioctl to get scsi disk capacity failed");
        }
        else {
          my_data->devSize = (off_t)capacity.lba * (off_t)DEV_BSIZE;
        }
      }
      else {
        my_data->devSize = (off_t)scsi_capacity.lba *
                           (off_t)scsi_capacity.blksz;
      }
      if (describe->dev_type == CDROM_DEV_TYPE) {
        ir_flag = 101;
      }
      else {
        if (my_data->scsi_immreport != -1) {
          if (ioctl(fd, SIOC_GET_IR, &ir_flag) == EIO) {
            if (test->debug) {
              fprintf(test->where, "%s: ioctl(SIOC_GET_IR) failed\n", __func__);
              fflush(test->where);
            }
          }
          if (!(ir_flag == 0 || ir_flag == 1)) {
            report_test_failure(test,
                                (char *)__func__,
                                DISK_TEST_SIOC_GET_IR_FAILED,
                                "ioctl SIOC_GET_IR returned illegal value");
          }
          else if (my_data->scsi_immreport != -1) {
            my_data->OrigImmReportValue = ir_flag;
            if (ioctl(fd, SIOC_SET_IR, my_data->scsi_immreport) < 0) {
              fprintf(test->where,
                      "Could not modify immediate report for device %s\n",
                      my_data->file_name);
              fflush(test->where);
              my_data->immReportValue = -1;
            }
          }
        }
        if (my_data->scsi_queue_depth != -1) {
          if (ioctl(fd, SIOC_GET_LUN_LIMITS, lun_limits) < 0) {
            fprintf(test->where,
                    "Could not get queue depth for device %s\n",
                    my_data->file_name);
            fflush(test->where);
            my_data->OrigQueueDepthValue = 0;
          }
          else {
            my_data->OrigQueueDepthValue = lun_limits->max_q_depth;
            (void) memset(lun_limits->reserved,0,sizeof(lun_limits->reserved));
            lun_limits->max_q_depth = my_data->scsi_queue_depth;
            lun_limits->flags = SCTL_ENABLE_TAGS;
            if (ioctl(fd, SIOC_SET_LUN_LIMITS, lun_limits) < 0) {
              fprintf(test->where,
                      "Could not set queue depth for device %s\n",
                      my_data->file_name);
              fflush(test->where);
              my_data->queueDepthValue = 0;
            }
            else {
              my_data->queueDepthValue = lun_limits->max_q_depth;
            }
          }
        }
      }
      break;
    case UNKNOWN_INTF:
    default:
      /* We have a known device type, but an unknown interface,
         so let's assume it is an lvm volume */
      if (ioctl(fd, DIOC_CAPACITY, &capacity) < 0) {
        report_test_failure(test,
                            (char *)__func__,
                            DISK_TEST_DIOC_CAPACITY_FAILED,
                            "ioctl DIOC_CAPACITY failed");
      }
      my_data->diskSize = (off_t)capacity.lba * my_data->sectSize;
      my_data->devSize  = (off_t)capacity.lba * my_data->sectSize;
      break;
    }
  }
  my_data->fd = fd;
}



static void
do_disk_warmup(test_t *test)
{
  disk_data_t  *my_data;

  void         *buf_addr;
  int           fd;
  off_t         i;
  size_t        size;
  off_t         where;
  off_t         xfer_size;

  /* get parameters from the test specific data area */
  my_data     = GET_TEST_DATA(test);
  buf_addr    = my_data->buffer_start;
  size        = my_data->chunk;
  where       = my_data->where;
  fd          = my_data->fd;
  xfer_size   = my_data->testSize;
  
  /* Perform a warmup I/O by reading 8K bytes from the disk.
   * This is needed to get the drive to perform any thermal 
   * recalibrations that it might need to do before the tests
   * begin.  Thermal recalibrations are bad because they can
   * cause an I/O to delay up to two seconds, causing significant
   * variation during testing.  This code insures that at least
   * 8K bytes of data have been read from the disk before entering
   * the load state. sgb 3/9/2006 */

  if (size > 8192) {
    size = 8192;
  }
  if (xfer_size > 8192) {
    xfer_size = 8192;
  }
  if (lseek(fd, where, SEEK_SET) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        DISK_TEST_SEEK_ERROR,
                        "lseek error");
  }
  for (i = 0; i < xfer_size; i += size) {
    io_read(fd, buf_addr, size, test, (char *)__func__);
  }
}


static uint32_t
do_raw_seq_disk_io(test_t *test)
{
  uint32_t      new_state;
  uint32_t      state;
  disk_data_t  *my_data;

  void         *buf_addr;
  int           fd;
  int           readIO;
  size_t        size;
  off_t         where;
  off_t         xfer_size;

  off_t         i;
  ssize_t       bytes;
       


  my_data = GET_TEST_DATA(test);
  state   = GET_TEST_STATE;

  /* get parameters from the test specific data area */
  buf_addr  = my_data->buffer_start;
  fd        = my_data->fd;
  size      = my_data->chunk;
  where     = my_data->where;
  xfer_size = my_data->testSize;
  
  if (test->debug) {
    fprintf(test->where, "%s: testSize = %lld\n", __func__, xfer_size);
    fprintf(test->where, "%s: size     = %ld\n",  __func__, size);
    fprintf(test->where, "%s: where    = %lld\n", __func__, where);
    fflush(test->where);
  }
  if (my_data->read != 0.0) {
    readIO = 1;
  }
  else {
    close(fd);
    readIO = 0;
    if ((fd = open(my_data->file_name, O_RDWR)) < 0) {
      fprintf(test->where,
              "%s: read/write open of file %s failed\n",
               __func__, my_data->file_name);
      fflush(test->where);
      report_test_failure(test,
                          (char *)__func__,
                          DISK_TEST_RDWR_OPEN_FAILED,
                          "read/write open of file_name failed");
    }
  }

  /* seek to the starting location on the disk */
  if (NO_STATE_CHANGE(test)) {
    if (lseek(fd, where, SEEK_SET) == -1) {
      report_test_failure(test,
                          (char *)__func__,
                          DISK_TEST_SEEK_ERROR,
                          "lseek error");
    }
  }

  i = 0;
  while (NO_STATE_CHANGE(test)) {
    HISTOGRAM_VARS;
    if (state == TEST_MEASURE) {
      HIST_TIMESTAMP(&time_one);
    }
    if (readIO) {
      bytes = io_read(fd, buf_addr, size, test, (char *)__func__);
      if (state == TEST_MEASURE) {
        if (my_data->read_hist) {
          HIST_TIMESTAMP(&time_two);
          HIST_ADD(my_data->read_hist, &time_one, &time_two);
        }
        my_data->stats.named.read_calls++;
        my_data->stats.named.bytes_read += bytes;
      }
    }
    else {
      bytes = io_write(fd, buf_addr, size, test, (char *)__func__);
      if (state == TEST_MEASURE) {
        if (my_data->write_hist) {
          HIST_TIMESTAMP(&time_two);
          HIST_ADD(my_data->write_hist, &time_one, &time_two);
        }
        my_data->stats.named.write_calls++;
        my_data->stats.named.bytes_written += bytes;
      }
    }
    i += bytes;
    if ( i >= xfer_size ) {
      /* seek to the starting location on the disk */
      if (lseek(fd, where, SEEK_SET) == -1) {
        report_test_failure(test,
                            (char *)__func__,
                            DISK_TEST_SEEK_ERROR,
                            "lseek error");
      }
      if (state == TEST_MEASURE) {
        my_data->stats.named.lseek_calls++;
      }
      i = 0;
    }
  }
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_LOADED) {
    /* transitioning to loaded state from measure state
       set current timestamp and update elapsed time */
    gettimeofday(&(my_data->curr_time), NULL);
    update_elapsed_time(my_data);
  }
  if (new_state == TEST_MEASURE) {
    /* transitioning to measure state from loaded state
       set previous timestamp */
    gettimeofday(&(my_data->prev_time), NULL);
  }
  return(new_state);
}



int
raw_seq_disk_io_clear_stats(test_t *test)
{
  return(disk_test_clear_stats(GET_TEST_DATA(test)));
}


xmlNodePtr
raw_seq_disk_io_get_stats(test_t *test)
{
  return( disk_test_get_stats(test));
}

void
raw_seq_disk_io_decode_stats(xmlNodePtr stats,test_t *test)
{
  disk_test_decode_stats(stats,test);
}


void
raw_seq_disk_io(test_t *test)
{
  uint32_t state, new_state;

  NETPERF_DEBUG_ENTRY(test->debug, test->where);

  disk_test_init(test);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        raw_disk_init(test);
        fix_up_parms(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        sleep(1);
      }
      if (new_state == TEST_LOADED) {
        do_disk_warmup(test);
      }
      break;
    case TEST_MEASURE:
    case TEST_LOADED:
      new_state = do_raw_seq_disk_io(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test,free_disk_test_data);

  NETPERF_DEBUG_EXIT(test->debug, test->where);

} /* end of seq_disk_io */



static void
disk_test_results_init(tset_t *test_set,char *report_flags,char *output)
{
  disk_results_t *rd;
  FILE           *outfd;
  int             max_count;
  size_t          malloc_size;

  rd        = test_set->report_data;
  max_count = test_set->confidence.max_count;

  if (output) {
    if (test_set->debug) {
      fprintf(test_set->where,
              "%s: report going to file %s\n",
              __func__, output);
      fflush(test_set->where);
    }
    outfd = fopen(output,"a");
  }
  else {
    if (test_set->debug) {
      fprintf(test_set->where,
              "%s: report going to file stdout\n",
              __func__);
      fflush(test_set->where);
    }
    outfd = stdout;
  }
  /* allocate and initialize report data */
  malloc_size = sizeof(disk_results_t) + 7 * max_count * sizeof(double);
  rd = malloc(malloc_size);
  if (rd) {

    /* original code took sizeof a math equation so memset only zeroed the */
    /* first sizeof(size_t) bytes.  This should work better  sgb 20060203  */

    memset(rd, 0, malloc_size);
    rd->max_count      = max_count;
    rd->iops           = &(rd->results_start);
    rd->read_iops      = &(rd->iops[max_count]);
    rd->write_iops     = &(rd->read_iops[max_count]);
    rd->read_results   = &(rd->write_iops[max_count]);
    rd->write_results  = &(rd->read_results[max_count]);
    rd->utilization    = &(rd->write_results[max_count]);
    rd->servdemand     = &(rd->utilization[max_count]);
    rd->run_time       = &(rd->servdemand[max_count]);
    rd->iops_minimum   = DBL_MAX;
    rd->iops_maximum   = DBL_MIN;
    rd->outfd          = outfd;
    rd->sd_denominator = 0.0;
    /* not all strcmp's play well with NULL pointers. bummer */
    if (NULL != report_flags) {
      if (!strcmp(report_flags,"PRINT_RUN")) {
        rd->print_run  = 1;
      }
      if (!strcmp(report_flags,"PRINT_TESTS")) {
        rd->print_test = 1;
      }
      if (!strcmp(report_flags,"PRINT_ALL")) {
        rd->print_hist    = 1;
        rd->print_run     = 1;
        rd->print_test    = 1;
        rd->print_per_cpu = 1;
      }
    }
    if (test_set->debug) {
      rd->print_hist    = 1;
      rd->print_run     = 1;
      rd->print_test    = 1;
      rd->print_per_cpu = 1;
    }
    test_set->report_data = rd;
  }
  else {
    /* could not allocate memory can't generate report */
    fprintf(outfd,
            "%s: malloc failed can't generate report\n",
            __func__);
    fflush(outfd);
    exit(-11);
  }
}

static void
process_test_stats(tset_t *test_set, xmlNodePtr stats, xmlChar *tid)
{
  int            i;
  int            count;
  int            index;
  FILE          *outfd;
  disk_results_t *rd;

  double         elapsed_seconds;
  double         iops;
  double         read_rate;
  double         write_rate;
  double         read_call_rate;
  double         write_call_rate;
  double         seek_call_rate;

#define TST_E_SEC         0
#define TST_E_USEC        1
#define TST_R_CALLS       4
#define TST_R_BYTES       5
#define TST_W_CALLS       6
#define TST_W_BYTES       7
#define TST_S_CALLS       8

#define MAX_TEST_CNTRS 9
  double         test_cntr[MAX_TEST_CNTRS];
  const char *cntr_name[] = {
    "elapsed_sec",
    "elapsed_usec",
    "time_sec",
    "time_usec",
    "cntr0_value",
    "cntr1_value",
    "cntr2_value",
    "cntr3_value",
    "cntr4_value",
  };

  rd     = test_set->report_data;
  count  = test_set->confidence.count;
  outfd  = rd->outfd;
  index  = count - 1;

  /* process test statistics */
  NETPERF_DEBUG_ENTRY(test_set->debug, test_set->where);

  for (i=0; i<MAX_TEST_CNTRS; i++) {
    char *value_str =
       (char *)xmlGetProp(stats, (const xmlChar *)cntr_name[i]);
    if (value_str) {
      test_cntr[i] = strtod(value_str,NULL);
      if (test_cntr[i] == 0.0) {
        uint64_t x;
        sscanf(value_str,"%llx",&x);
        test_cntr[i] = (double)x;
      }
    }
    else {
      test_cntr[i] = 0.0;
    }
    if (test_set->debug) {
      unsigned char *string=NULL;
      string = xmlGetProp(stats, (const xmlChar *)cntr_name[i]);
      fprintf(test_set->where,"\t%12s test_cntr[%2d] = %10g\t'%s'\n",
              cntr_name[i], i, test_cntr[i],
	      string ? (char *)string : "n/a");
    }
  }
  elapsed_seconds  = test_cntr[TST_E_SEC] + test_cntr[TST_E_USEC]/1000000.0;
  read_rate        = test_cntr[TST_R_BYTES] / (elapsed_seconds*1024.0*1024.0);
  write_rate       = test_cntr[TST_W_BYTES] / (elapsed_seconds*1024.0*1024.0);
  read_call_rate   = test_cntr[TST_R_CALLS] / elapsed_seconds;
  write_call_rate  = test_cntr[TST_W_CALLS] / elapsed_seconds;
  seek_call_rate   = test_cntr[TST_S_CALLS] / elapsed_seconds;
  iops             = read_call_rate + write_call_rate;
  if (test_set->debug) {
    fprintf(test_set->where,"\tread_rate = %7g\t%7g\n",
            read_rate, test_cntr[TST_R_BYTES]);
    fprintf(test_set->where,"\twrite_rate = %7g\t%7g\n",
            write_rate, test_cntr[TST_W_BYTES]);
    fprintf(test_set->where,"\tread_call_rate = %7g\t%7g\n",
            read_call_rate, test_cntr[TST_R_CALLS]);
    fprintf(test_set->where,"\twrite_call_rate = %7g\t%7g\n",
            write_call_rate, test_cntr[TST_W_CALLS]);
    fprintf(test_set->where,"\tseek_call_rate = %7g\t%7g\n",
            seek_call_rate, test_cntr[TST_S_CALLS]);
    fflush(test_set->where);
  }
  if (rd->sd_denominator == 0.0) {
    rd->sd_denominator = 1024.0*1024.0/1024.0;
  }
  if (test_set->debug) {
    fprintf(test_set->where,"\tsd_denominator = %f\n",rd->sd_denominator);
    fflush(test_set->where);
  }
  /* accumulate results for the run */
  rd->run_time[index]        += elapsed_seconds;
  rd->read_results[index]    += read_rate;
  rd->write_results[index]   += write_rate;
  rd->read_iops[index]       += read_call_rate;
  rd->write_iops[index]      += write_call_rate;
  rd->iops[index]            += iops;

  if (rd->print_hist) {
    xmlNodePtr  hist;
    hist = stats->xmlChildrenNode;
    if (hist == NULL) {
      fprintf(outfd,"%s:No Histogram Node Found\n\n",__func__);
      fflush(outfd);
    }
    while (hist != NULL) {
      if (!xmlStrcmp(hist->name,(const xmlChar *)"hist_stats")) {
        HIST_REPORT(outfd, hist);
      }
      hist = hist->next;
    }
  }

  if (rd->print_test) {
    /* Display per test results */
    fprintf(outfd,"%3d  ", count);                    /*  0,5 */
    fprintf(outfd,"%-6s ",  tid);                     /*  5,7 */
    fprintf(outfd,"%-6.2f ",elapsed_seconds);         /* 12,7 */
    fprintf(outfd,"%7.2f ",iops);                     /* 19,8 */
    fprintf(outfd,"%7.2f ",read_call_rate);           /* 27.8 */
    fprintf(outfd,"%7.2f ",write_call_rate);          /* 35,8 */
    fprintf(outfd,"%7.2f ",read_rate);                /* 43,8 */
    fprintf(outfd,"%7.2f ",write_rate);               /* 51,8 */
    fprintf(outfd,"%7.4f ",seek_call_rate);           /* 59,8 */
    fprintf(outfd,"%7.2f ",iops/seek_call_rate);      /* 67,8 */
    fprintf(outfd,"\n");
    fflush(outfd);
  }
  /* end of printing disk per test instance results */
}

static double
process_sys_stats(tset_t *test_set, xmlNodePtr stats, xmlChar *tid)
{
  int             i;
  int             count;
  int             index;
  FILE           *outfd;
  disk_results_t *rd;
  double          elapsed_seconds;
  double          sys_util;
  double          calibration;
  double          local_idle;
  double          local_busy;
  double          local_cpus;

#define MAX_SYS_CNTRS 10
#define E_SEC         0
#define E_USEC        1
#define NUM_CPU       4
#define CALIBRATE     5
#define IDLE          6

  double         sys_cntr[MAX_SYS_CNTRS];
  const char *sys_cntr_name[] = {
    "elapsed_sec",
    "elapsed_usec",
    "time_sec",
    "time_usec",
    "number_cpus",
    "calibration",
    "idle_count",
    "",
    "",
    ""
  };

  rd     = test_set->report_data;
  count  = test_set->confidence.count;
  outfd  = rd->outfd;
  index  = count - 1;

  if (test_set->debug) {
    fprintf(test_set->where,"\tprocessing sys_stats\n");
    fflush(test_set->where);
  }
  for (i=0; i<MAX_SYS_CNTRS; i++) {
    char *value_str =
       (char *)xmlGetProp(stats, (const xmlChar *)sys_cntr_name[i]);
    if (value_str) {
      sys_cntr[i] = strtod(value_str,NULL);
      if (sys_cntr[i] == 0.0) {
        uint64_t x;
        sscanf(value_str,"%llx",&x);
        sys_cntr[i] = (double)x;
      }
    }
    else {
      sys_cntr[i] = 0.0;
    }
    if (test_set->debug) {
      unsigned char *string=NULL;
      string = xmlGetProp(stats, (const xmlChar *)sys_cntr_name[i]);
      fprintf(test_set->where,"\t%12s sys_stats[%d] = %10g '%s'\n",
              sys_cntr_name[i], i, sys_cntr[i],
	      string ? (char *)string : "n/a");
    }
  }
  local_cpus      = sys_cntr[NUM_CPU];
  elapsed_seconds = sys_cntr[E_SEC] + sys_cntr[E_USEC]/1000000;
  calibration     = sys_cntr[CALIBRATE];
  local_idle      = sys_cntr[IDLE] / calibration;
  local_busy      = (calibration-sys_cntr[IDLE])/calibration;

  if (test_set->debug) {
    fprintf(test_set->where,"\tnum_cpus        = %f\n",local_cpus);
    fprintf(test_set->where,"\telapsed_seconds = %7.2f\n",elapsed_seconds);
    fprintf(test_set->where,"\tidle_cntr       = %e\n",sys_cntr[IDLE]);
    fprintf(test_set->where,"\tcalibrate_cntr  = %e\n",sys_cntr[CALIBRATE]);
    fprintf(test_set->where,"\tlocal_idle      = %e\n",local_idle);
    fprintf(test_set->where,"\tlocal_busy      = %e\n",local_busy);
    fflush(test_set->where);
  }
  rd->utilization[index]  += local_busy;

  if (rd->print_per_cpu) {
    xmlNodePtr  cpu;
    char       *value_str;

    cpu = stats->xmlChildrenNode;
    while (cpu != NULL) {
      if (!xmlStrcmp(cpu->name,(const xmlChar *)"per_cpu_stats")) {
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"cpu_id");
        fprintf(outfd,"  cpu%2s ", value_str);
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"calibration");
        fprintf(outfd,"%s ", value_str);
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"idle_count");
        fprintf(outfd,"%s ", value_str);
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"user_count");
        fprintf(outfd,"%s ", value_str);
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"sys_count");
        fprintf(outfd,"%s ", value_str);
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"int_count");
        fprintf(outfd,"%s ", value_str);
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"other_count");
        fprintf(outfd,"%s\n", value_str);
        fflush(outfd);
      }
      cpu = cpu->next;
    }
  }

  if (rd->print_test) {
    /* Display per test results */
    fprintf(outfd,"%3d  ", count);                    /*  0,5 */
    fprintf(outfd,"%-6s ",  tid);                     /*  5,7 */
    fprintf(outfd,"%-6.2f ",elapsed_seconds);         /* 12,7 */
      fprintf(outfd,"%24s","");                       /* 19,24*/
    fprintf(outfd,"%7.1e ",calibration);              /* 43,8 */
    fprintf(outfd,"%6.3f ",local_idle*100.0);         /* 51,7 */
    fprintf(outfd,"%6.3f ",local_busy*100.0);         /* 58,7 */
    fprintf(outfd,"\n");                              /* 79,1 */
    fflush(outfd);
  }
  /* end of printing sys stats instance results */
  return(local_cpus);
}

static void
process_stats_for_run(tset_t *test_set)
{
  disk_results_t *rd;
  test_t         *test;
  tset_elt_t     *set_elt;
  xmlNodePtr      stats;
  xmlNodePtr      prev_stats;
  int             count;
  int             index;
  int             num_of_tests;
  double          num_of_cpus;
  double          run_results;


  rd        = test_set->report_data;
  set_elt   = test_set->tests;
  count     = test_set->confidence.count;
  index     = count - 1;

  if (test_set->debug) {
    fprintf(test_set->where,
            "test_set %s has %d tests looking for statistics\n",
            test_set->id,test_set->num_tests);
    fflush(test_set->where);
  }

  if (test_set->debug) {
    fprintf(test_set->where, "%s count = %d\n", __func__, count);
    fflush(test_set->where);
  }

  rd->iops[index]          =  0.0;
  rd->read_results[index]  =  0.0;
  rd->write_results[index] =  0.0;
  rd->utilization[index]   =  0.0;
  rd->servdemand[index]    =  0.0;
  rd->run_time[index]      =  0.0;

  num_of_tests  = 0;
  num_of_cpus   = 0.0;
  while (set_elt != NULL) {
    int stats_for_test;
    test    = set_elt->test;
    stats   = test->received_stats->xmlChildrenNode;
    if (test_set->debug) {
      if (stats) {
        fprintf(test_set->where,
                "\ttest %s has '%s' statistics\n",
                test->id,stats->name);
      }
      else {
        fprintf(test_set->where,
                "\ttest %s has no statistics available!\n",
                test->id);
      }
      fflush(test_set->where);
    }
    stats_for_test = 0;
    while (stats != NULL) {
      /* process all the statistics records for this test */
      if (test_set->debug) {
        fprintf(test_set->where,"\tfound some statistics");
        fflush(test_set->where);
      }
      if(!xmlStrcmp(stats->name,(const xmlChar *)"sys_stats")) {
        /* process system statistics */
        num_of_cpus = process_sys_stats(test_set, stats, test->id);
        stats_for_test++;
      }
      if(!xmlStrcmp(stats->name,(const xmlChar *)"test_stats")) {
        /* process test statistics */
        process_test_stats(test_set, stats, test->id);
        stats_for_test++;
        num_of_tests++;
      }
      /* other types of nodes just get skipped by this report routine */
      /* delete statistics entry from test */
      prev_stats = stats;
      stats = stats->next;
      xmlUnlinkNode(prev_stats);
      xmlFreeNode(prev_stats);
    }
    /* should only have one stats record for each test otherwise error */
    if (stats_for_test > 1) {
      /* someone is playing games don't generate report*/
      fprintf(test_set->where,
              "More than one statistics measurement for test %d\n",
              stats_for_test);
      fprintf(test_set->where,
              "%s was not designed to deal with this.\n",
              __func__);
      fprintf(test_set->where,
              "exiting netperf now!!\n");
      fflush(test_set->where);
      exit(-13);
    }
    set_elt = set_elt->next;
  }

  if (rd->iops_minimum > rd->iops[index]) {
    rd->iops_minimum = rd->iops[index];
  }
  if (rd->iops_maximum < rd->iops[index]) {
    rd->iops_maximum = rd->iops[index];
  }
  rd->run_time[index] = rd->run_time[index] / (double)num_of_tests;

  /* now calculate service demand for this test run. Remember the cpu */
  /* utilization is in the range 0.0 to 1.0 so we need to multiply by */
  /* the number of cpus and 1,000,000.0 to get to microseconds of cpu */
  /* time per unit of work.  Service demand is calculated on the sum  */
  /* of read and write results which are in 2^20 bytes per second.    */
  /* the sd_denominator is factored in to convert service demand into */
  /* usec/Kbytes.                                                     */

  run_results = rd->read_results[index] + rd->write_results[index];
  if ((run_results != 0.0) && (num_of_cpus != 0.0)) {
    rd->servdemand[index] = rd->utilization[index] * num_of_cpus * 1000000.0 /
                            (run_results * rd->sd_denominator);
  }
  NETPERF_DEBUG_EXIT(test_set->debug,test_set->where);
}


static void
update_results_and_confidence(tset_t *test_set)
{
  disk_results_t *rd;
  double          confidence;
  double          temp;
  int             loc_debug = 0;

  rd        = test_set->report_data;

  NETPERF_DEBUG_ENTRY(test_set->debug,test_set->where);

  /* calculate confidence and summary result values */
  rd->read_confidence           = get_confidence(rd->read_results,
                                      &(test_set->confidence),
                                      &(rd->read_measured_mean),
                                      &(rd->read_interval));
  if (test_set->debug || loc_debug) {
    fprintf(test_set->where,
            "\tread_results conf = %.2f%%\tmean = %10f +/- %8f\n",
            100.0 * rd->read_confidence,
            rd->read_measured_mean,
            rd->read_interval);
    fflush(test_set->where);
  }
  rd->write_confidence          = get_confidence(rd->write_results,
                                      &(test_set->confidence),
                                      &(rd->write_measured_mean),
                                      &(rd->write_interval));
  if (test_set->debug || loc_debug) {
    fprintf(test_set->where,
            "\twrite_results conf = %.2f%%\tmean = %10f +/- %8f\n",
            100.0 * rd->write_confidence,
            rd->write_measured_mean,
            rd->write_interval);
    fflush(test_set->where);
  }
  confidence                    = get_confidence(rd->run_time,
                                      &(test_set->confidence),
                                      &(rd->ave_time),
                                      &(temp));
  rd->iops_confidence           = get_confidence(rd->iops,
                                      &(test_set->confidence),
                                      &(rd->iops_measured_mean),
                                      &(rd->iops_interval));
  if (test_set->debug || loc_debug) {
    fprintf(test_set->where,
            "\tiops      conf = %.2f%%\tmean = %10f +/- %8f\n",
            100.0 * rd->iops_confidence,
            rd->iops_measured_mean,
            rd->iops_interval);
    fflush(test_set->where);
  }
  rd->cpu_util_confidence       = get_confidence(rd->utilization,
                                      &(test_set->confidence),
                                      &(rd->cpu_util_measured_mean),
                                      &(rd->cpu_util_interval));
  if (test_set->debug || loc_debug) {
    fprintf(test_set->where,
            "\tcpu_util     conf = %.2f%%\tmean = %10f +/- %8f\n",
            100.0 * rd->cpu_util_confidence,
            rd->cpu_util_measured_mean,
            rd->cpu_util_interval);
    fflush(test_set->where);
  }
  rd->service_demand_confidence = get_confidence(rd->servdemand,
                                      &(test_set->confidence),
                                      &(rd->service_demand_measured_mean),
                                      &(rd->service_demand_interval));
  if (test_set->debug || loc_debug) {
    fprintf(test_set->where,
            "\tserv_demand  conf = %.2f%%\tmean = %10f +/- %8f\n",
            100.0 * rd->service_demand_confidence,
            rd->service_demand_measured_mean,
            rd->service_demand_interval);
    fflush(test_set->where);
  }

  if (rd->iops_confidence >  rd->cpu_util_confidence) {
    confidence = rd->iops_confidence;
  }
  else {
    confidence = rd->cpu_util_confidence;
  }
  if (rd->service_demand_confidence > confidence) {
    confidence = rd->service_demand_confidence;
  }
  if (rd->write_measured_mean > 0.0) {
    if (rd->write_confidence > confidence) {
      confidence = rd->write_confidence;
    }
  }
  if (rd->read_measured_mean > 0.0) {
    if (rd->read_confidence > confidence) {
      confidence = rd->read_confidence;
    }
  }

  if (test_set->confidence.min_count > 1) {
    test_set->confidence.value = test_set->confidence.interval - confidence;
    if (test_set->debug || loc_debug) {
      fprintf(test_set->where,
              "\t%3d run confidence = %.2f%%\tcheck value = %f\n",
              test_set->confidence.count,
              100.0 * confidence, test_set->confidence.value);
      fflush(test_set->where);
    }
  }
  NETPERF_DEBUG_EXIT(test_set->debug,test_set->where);
}


static void
print_run_results(tset_t *test_set)
{
  disk_results_t *rd;
  FILE           *outfd;
  int             i;
  int             count;
  int             index;

#define HDR_LINES 3
  char *field1[HDR_LINES]   = { "INT",   "NUM",  " "       };   /* 3 */
  char *field2[HDR_LINES]   = { "SET",   "Name", " "       };   /* 4 */
  char *field3[HDR_LINES]   = { "RUN",   "Time", "sec"     };   /* 6 */
  char *field4[HDR_LINES]   = { "I/0",   "RATE", "io/s"    };   /* 7 */
  char *field5[HDR_LINES]   = { "READ",  "Rate", "MB/s"    };   /* 6 */
  char *field6[HDR_LINES]   = { "WRT",   "Rate", "MB/s"    };   /* 6 */
  char *field7[HDR_LINES]   = { "CPU",   "Util", "%/100"   };   /* 6 */
  char *field8[HDR_LINES]   = { "SD",    "usec", "/KB"     };   /* 6 */
  char *field9[HDR_LINES]   = { "conf",  "+/-",  "io/s"    };   /* 5 */
  char *field10[HDR_LINES]  = { "conf",  "+/-",  "MB/s"    };   /* 5 */
  char *field11[HDR_LINES]  = { "conf",  "+/-",  "MB/s"    };   /* 5 */
  char *field12[HDR_LINES]  = { "+/-",   "Util", "%/100"   };   /* 6 */
  char *field13[HDR_LINES]  = { "+/-",   "usec", "/KB"     };   /* 6 */

  rd    = test_set->report_data;
  count = test_set->confidence.count;
  outfd = rd->outfd;
  index = count - 1;


  /* Display per run header */
  fprintf(outfd,"\n");
  for (i=0;i < HDR_LINES; i++) {
    fprintf(outfd,"%-3s ",field1[i]);                         /*  0,4  */
    fprintf(outfd,"%-4s ",field2[i]);                         /*  4,5  */
    fprintf(outfd,"%-6s ",field3[i]);                         /*  9,7  */
    fprintf(outfd,"%9s ",field4[i]);                          /* 16,10 */
    if (rd->read_results[index] > 0.0) {
      fprintf(outfd,"%7s ",field5[i]);                        /* 26,8  */
    }
    if (rd->write_results[index] > 0.0) {
      fprintf(outfd,"%7s ",field6[i]);                        /* 34,8  */
    }
    fprintf(outfd,"%7s ",field7[i]);                          /* 42,8  */
    fprintf(outfd,"%7s ",field8[i]);                          /* 45,8  */
    if (index > 0) {
      fprintf(outfd,"%6s ",field9[i]);                        /* 52,7  */
      if (rd->read_results[index] > 0.0) {
        fprintf(outfd,"%6s ",field10[i]);                     /* 58,7  */
      }
      if (rd->write_results[index] > 0.0) {
        fprintf(outfd,"%6s ",field11[i]);                     /* 64,7  */
      }
      fprintf(outfd,"%7s ",field12[i]);                       /* 70,8  */
      fprintf(outfd,"%7s", field13[i]);                       /* 87,8  */
    }
    fprintf(outfd,"\n");
  }

  /* Display per run results */
  fprintf(outfd,"%-3d ", count);                              /*  0,4  */
  fprintf(outfd,"%-4s ",test_set->id);                        /*  4,5  */
  fprintf(outfd,"%-6.2f ",rd->run_time[index]);               /*  9,7  */
  fprintf(outfd,"%9.2f ",rd->iops[index]);                    /* 16,10 */
  if (rd->read_results[index] > 0.0) {
    fprintf(outfd,"%7.2f ",rd->read_results[index]);          /* 26,8  */
  }
  if (rd->write_results[index] > 0.0) {
    fprintf(outfd,"%7.2f ",rd->write_results[index]);         /* 34,8  */
  }
  fprintf(outfd,"%7.5f ",rd->utilization[index]);             /* 42,8  */
  fprintf(outfd,"%7.3f ",rd->servdemand[index]);              /* 50,8  */
  if (index > 0) {
    fprintf(outfd,"%6.2f ",rd->iops_interval);                /* 58,7  */
    if (rd->read_results[index] > 0.0) {
      fprintf(outfd,"%6.3f ",rd->read_interval);              /* 65,7  */
    }
    if (rd->write_results[index] > 0.0) {
      fprintf(outfd,"%6.3f ",rd->write_interval);             /* 72,7  */
    }
    fprintf(outfd,"%7.5f ",rd->cpu_util_interval);            /* 79,8  */
    fprintf(outfd,"%7.4f", rd->service_demand_interval);      /* 87,8  */
  }
  fprintf(outfd,"\n");
  fflush(outfd);
}


static void
print_did_not_meet_confidence(tset_t *test_set)
{
  disk_results_t *rd;
  FILE           *outfd;

  rd    = test_set->report_data;
  outfd = rd->outfd;


  /* print the confidence failed line */
  fprintf(outfd,"\n");
  fprintf(outfd,"!!! WARNING\n");
  fprintf(outfd,"!!! Desired confidence was not achieved within ");
  fprintf(outfd,"the specified iterations. (%d)\n",
          test_set->confidence.max_count);
  fprintf(outfd,
          "!!! This implies that there was variability in ");
  fprintf(outfd,
          "the test environment that\n");
  fprintf(outfd,
          "!!! must be investigated before going further.\n");
  fprintf(outfd,
          "!!! Confidence intervals: IOP_RATE   : %6.2f%%\n",
          100.0 * rd->iops_confidence);
  if (rd->read_measured_mean > 0.0) {
    fprintf(outfd,
          "!!! Confidence intervals: READ_RATE  : %6.2f%%\n",
          100.0 * rd->read_confidence);
  }
  if (rd->write_measured_mean > 0.0) {
    fprintf(outfd,
          "!!! Confidence intervals: WRITE_RATE : %6.2f%%\n",
          100.0 * rd->write_confidence);
  }
  fprintf(outfd,
          "!!!                       CPU util   : %6.2f%%\n",
          100.0 * rd->cpu_util_confidence);
  fprintf(outfd,
          "!!!                       ServDemand : %6.2f%%\n",
          100.0 * rd->service_demand_confidence);
  fflush(outfd);
}


static void
print_results_summary(tset_t *test_set)
{
  disk_results_t *rd;
  FILE           *outfd;
  int             i;

#define HDR_LINES 3
                                                                /* field
                                                                   width*/
  char *field1[HDR_LINES]   = { "AVE",   " / ",  "Num"     };   /* 3 */
  char *field2[HDR_LINES]   = { "SET",   "Name", " "       };   /* 4 */
  char *field3[HDR_LINES]   = { "TEST",  "Time", "sec"     };   /* 6 */
  char *field4[HDR_LINES]   = { "I/O",   "RATE", "io/s"    };   /* 7 */
  char *field5[HDR_LINES]   = { "conf",  "+/-",  "io/s"    };   /* 5 */
  char *field6[HDR_LINES]   = { "READ",  "Rate", "MB/s"    };   /* 6 */
  char *field7[HDR_LINES]   = { "conf",  "+/-",  "MB/s"    };   /* 5 */
  char *field8[HDR_LINES]   = { "WRT",   "Rate", "MB/s"    };   /* 6 */
  char *field9[HDR_LINES]   = { "conf",  "+/-",  "MB/s"    };   /* 5 */
  char *field10[HDR_LINES]  = { "CPU",   "Util", "%/100"   };   /* 6 */
  char *field11[HDR_LINES]  = { "+/-",   "Util", "%/100"   };   /* 6 */
  char *field12[HDR_LINES]  = { "SD",    "usec", "/KB"     };   /* 6 */
  char *field13[HDR_LINES]  = { "+/-",   "usec", "/KB"     };   /* 6 */

  rd    = test_set->report_data;
  outfd = rd->outfd;

  /* Print the summary header */
  fprintf(outfd,"\n");
  for (i = 0; i < HDR_LINES; i++) {
    fprintf(outfd,"%-3s ",field1[i]);                             /*  0,4  */
    fprintf(outfd,"%-4s ",field2[i]);                             /*  4,5  */
    fprintf(outfd,"%-6s ",field3[i]);                             /*  9,7  */
    fprintf(outfd,"%9s ",field4[i]);                              /* 16,10 */
    fprintf(outfd,"%6s ",field5[i]);                              /* 26,7  */
    if (rd->read_measured_mean > 0.0) {
      fprintf(outfd,"%7s ",field6[i]);                            /* 33,8  */
      fprintf(outfd,"%6s ",field7[i]);                            /* 41,7  */
    }
    if (rd->write_measured_mean > 0.0) {
      fprintf(outfd,"%7s ",field8[i]);                            /* 48,8  */
      fprintf(outfd,"%6s ",field9[i]);                            /* 56,7  */
    }
    fprintf(outfd,"%7s ",field10[i]);                             /* 65,8  */
    fprintf(outfd,"%7s ",field11[i]);                             /* 73,8  */
    fprintf(outfd,"%7s ",field12[i]);                             /* 81,8  */
    fprintf(outfd,"%7s", field13[i]);                             /* 89,8  */
    fprintf(outfd,"\n");
  }

  /* print the summary results line */
  fprintf(outfd,"A%-2d ",test_set->confidence.count);             /*  0,4  */
  fprintf(outfd,"%-4s ",test_set->id);                            /*  4,5  */
  fprintf(outfd,"%-6.2f ",rd->ave_time);                          /*  9,7  */
  fprintf(outfd,"%9.2f ",rd->iops_measured_mean);                 /* 16,10 */
  fprintf(outfd,"%6.2f ",rd->iops_interval);                      /* 26,7  */
  if (rd->read_measured_mean > 0.0) {
    fprintf(outfd,"%7.2f ",rd->read_measured_mean);               /* 33,8  */
    fprintf(outfd,"%6.3f ",rd->read_interval);                    /* 41,7  */
  }
  if (rd->write_measured_mean > 0.0) {
    fprintf(outfd,"%7.2f ",rd->write_measured_mean);              /* 48,8  */
    fprintf(outfd,"%6.3f ",rd->write_interval);                   /* 56,7  */
  }
  fprintf(outfd,"%7.5f ",rd->cpu_util_measured_mean);             /* 65,8  */
  fprintf(outfd,"%7.5f ",rd->cpu_util_interval);                  /* 73,8  */
  fprintf(outfd,"%7.3f ",rd->service_demand_measured_mean);       /* 81,8  */
  fprintf(outfd,"%7.4f", rd->service_demand_interval);            /* 89,8  */
  fprintf(outfd,"\n");
  fflush(outfd);
}


void
report_disk_test_results(tset_t *test_set, char *report_flags, char *output)
{
  disk_results_t *rd;
  int count;
  int max_count;
  int min_count;

  rd  = test_set->report_data;

  if (rd == NULL) {
    disk_test_results_init(test_set,report_flags,output);
    rd  = test_set->report_data;
  }

  /* process statistics for this run */
  process_stats_for_run(test_set);

  /* calculate confidence and summary result values */
  update_results_and_confidence(test_set);

  if (rd->print_run) {
    print_run_results(test_set);
  }

  count        = test_set->confidence.count;
  max_count    = test_set->confidence.max_count;
  min_count    = test_set->confidence.min_count;

  /* always print summary results at end of last call through loop */
  if ((count >= max_count) ||
      ((test_set->confidence.value >= 0) && (count >= min_count))) {
    print_results_summary(test_set);
    if ((test_set->confidence.value < 0) && (min_count > 1)) {
      print_did_not_meet_confidence(test_set);
    }
  }
} /* end of report_disk_test_results */
