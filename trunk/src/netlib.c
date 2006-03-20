char netlib_id[]="\
@(#)netlib.c (c) Copyright 2005, Hewlett-Packard Company, $Id$";

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

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#ifdef WITH_GLIB
# include <glib.h>
# include <gmodule.h>
#else
# ifdef HAVE_PTHREAD_H
#  include <pthread.h>
# endif
#endif

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#define NETLIB

#include "netperf.h"

#undef NETLIB

#include "netlib.h"

extern int debug;
extern FILE * where;
extern test_hash_t test_hash[TEST_HASH_BUCKETS];
extern tset_hash_t test_set_hash[TEST_SET_HASH_BUCKETS];

#include "hist.h"

HIST
HIST_new(void){
  HIST h;
  h = (HIST) malloc(sizeof(struct histogram_struct));
  if (h) {
    HIST_clear(h);
  }
  return h;
}

void
HIST_clear(HIST h){
  int i;
  for(i = 0; i < 10; i++){
#ifdef HAVE_GETHRTIME
    h->hundred_nsec[i] = 0;
#endif
    h->unit_usec[i] = 0;
    h->ten_usec[i] = 0;
    h->hundred_usec[i] = 0;
    h->unit_msec[i] = 0;
    h->ten_msec[i] = 0;
    h->hundred_msec[i] = 0;
    h->unit_sec[i] = 0;
    h->ten_sec[i] = 0;
  }
  h->ridiculous = 0;
  h->total = 0;
}

#ifdef HAVE_GETHRTIME

void
HIST_add_nano(register HIST h, hrtime_t *begin, hrtime_t *end)
{
  register int64_t val;

  val = (*end) - (*begin);
  h->total++;
  val = val/100;
  if (val <= 9) h->hundred_nsec[val]++;
  else {
    val = val/10;
    if (val <= 9) h->unit_usec[val]++;
    else {
      val = val/10;
      if (val <= 9) h->ten_usec[val]++;
      else {
        val = val/10;
        if (val <= 9) h->hundred_usec[val]++;
        else {
          val = val/10;
          if (val <= 9) h->unit_msec[val]++;
          else {
            val = val/10;
            if (val <= 9) h->ten_msec[val]++;
            else {
              val = val/10;
              if (val <= 9) h->hundred_msec[val]++;
              else {
                val = val/10;
                if (val <= 9) h->unit_sec[val]++;
                else {
                  val = val/10;
                  if (val <= 9) h->ten_sec[val]++;
                  else h->ridiculous++;
                }
              }
            }
          }
        }
      }
    }
  }
}

#endif


void
HIST_add(register HIST h, int time_delta)
{
  register int val;
  h->total++;
  val = time_delta;
  if(val <= 9) h->unit_usec[val]++;
  else {
    val = val/10;
    if(val <= 9) h->ten_usec[val]++;
    else {
      val = val/10;
      if(val <= 9) h->hundred_usec[val]++;
      else {
        val = val/10;
        if(val <= 9) h->unit_msec[val]++;
        else {
          val = val/10;
          if(val <= 9) h->ten_msec[val]++;
          else {
            val = val/10;
            if(val <= 9) h->hundred_msec[val]++;
            else {
              val = val/10;
              if(val <= 9) h->unit_sec[val]++;
              else {
                val = val/10;
                if(val <= 9) h->ten_sec[val]++;
                else h->ridiculous++;
              }
            }
          }
        }
      }
    }
  }
}


static xmlAttrPtr
set_hist_attribute(xmlNodePtr hist, char *name, int *row)
{
  int         i,j;
  xmlAttrPtr  ap   = NULL;
  char        values[256];
  
  values[0] = 0;
  for (i = 0, j = 0; i < 10; i++) {
    j += snprintf(&(values[j]), 256-j, ":%5d", row[i]);
  }
  ap = xmlSetProp(hist, (xmlChar *)name, (xmlChar *)values);
  return(ap);
}


xmlNodePtr
HIST_stats_node(HIST h, char *name)
{
  int         i,j;
  xmlNodePtr  hist;
  xmlAttrPtr  ap;
  char        value_str[32];
  

  if ((hist = xmlNewNode(NULL,(xmlChar *)"hist_stats")) != NULL) {
    ap = xmlSetProp(hist, (xmlChar *)"hist_name", (xmlChar *)name);
#ifdef HAVE_GETHRTIME
    if (ap != NULL) {
      ap = set_hist_attribute(hist, "hundred_nsec", h->hundred_nsec);
    }
#endif
    if (ap != NULL) {
      ap = set_hist_attribute(hist, "unit_usec", h->unit_usec);
    }
    if (ap != NULL) {
      ap = set_hist_attribute(hist, "ten_usec", h->ten_usec);
    }
    if (ap != NULL) {
      ap = set_hist_attribute(hist, "hundred_usec", h->hundred_usec);
    }
    if (ap != NULL) {
      ap = set_hist_attribute(hist, "unit_msec", h->unit_msec);
    }
    if (ap != NULL) {
      ap = set_hist_attribute(hist, "ten_msec", h->ten_msec);
    }
    if (ap != NULL) {
      ap = set_hist_attribute(hist, "hundred_msec", h->hundred_msec);
    }
    if (ap != NULL) {
      ap = set_hist_attribute(hist, "unit_sec", h->unit_sec);
    }
    if (ap != NULL) {
      ap = set_hist_attribute(hist, "ten_sec", h->ten_sec);
    }
    if (ap != NULL) {
      sprintf(value_str,": %4d",h->ridiculous);
      ap = xmlSetProp(hist, (xmlChar *)"plus_100_sec", (xmlChar *)value_str);
    }
    if (ap != NULL) {
      sprintf(value_str,": %4lld",h->total);
      ap = xmlSetProp(hist, (xmlChar *)"hist_total", (xmlChar *)value_str);
    }
    if (ap == NULL) {
      xmlFreeNode(hist);
      hist = NULL;
    }
  }
  return(hist);
}


void
HIST_report(FILE *fd, xmlNodePtr hist)
{
   int         i;
   xmlChar    *string;
   
   string = xmlGetProp(hist, (const xmlChar *)"hist_name");
   fprintf(fd, "\nHISTOGRAM REPORT for %s\n", string);
   fprintf(fd, "              ");
   for (i=0; i<10; i++) {
     fprintf(fd, ":%5d",i);
   }
   fprintf(fd, "\n");
   fprintf(fd, "--------------");
   for (i=0; i<10; i++) {
     fprintf(fd, "+-----");
   }
   fprintf(fd, "\n");
   string = xmlGetProp(hist, (const xmlChar *)"hundred_nsec");
   if (string) {
     fprintf(fd, "HUNDRED_NSEC  %s\n", string);
   }
   string = xmlGetProp(hist, (const xmlChar *)"unit_usec");
   fprintf(fd, "UNIT_USEC     %s\n", string);
   string = xmlGetProp(hist, (const xmlChar *)"ten_usec");
   fprintf(fd, "TEN_USEC      %s\n", string);
   string = xmlGetProp(hist, (const xmlChar *)"hundred_usec");
   fprintf(fd, "HUNDRED_USEC  %s\n", string);
   string = xmlGetProp(hist, (const xmlChar *)"unit_msec");
   fprintf(fd, "UNIT_MSEC     %s\n", string);
   string = xmlGetProp(hist, (const xmlChar *)"ten_msec");
   fprintf(fd, "TEN_MSEC      %s\n", string);
   string = xmlGetProp(hist, (const xmlChar *)"hundred_msec");
   fprintf(fd, "HUNDRED_MSEC  %s\n", string);
   string = xmlGetProp(hist, (const xmlChar *)"unit_sec");
   fprintf(fd, "UNIT_SEC      %s\n", string);
   string = xmlGetProp(hist, (const xmlChar *)"ten_sec");
   fprintf(fd, "TEN_SEC       %s\n", string);
   string = xmlGetProp(hist, (const xmlChar *)"plus_100_sec");
   fprintf(fd, "100_PLUS_SEC  %s\n", string);
   string = xmlGetProp(hist, (const xmlChar *)"hist_total");
   fprintf(fd, "HIST_TOTAL    %s\n", string);
   fprintf(fd, "\n");
   fflush(fd);
}


#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

/* given a filename, return the first path to the file that stats
   successfully - which is either the name already given, or that name
   with NETPERFDIR prepended. */

int
netperf_complete_filename(char *name, char *full, int fulllen) {
  char path[PATH_MAX+1];
  int  ret;

  struct stat buf;

  if (0 == NETPERF_STAT(name,&buf)) {
    /* the name resolved where we are - either there is a match in
       CWD, or they specified a full path name, I wonder if we need to
       worry about explicit null termination of full? raj 2006-02-27 */
    strncpy(full,name,fulllen);
    full[fulllen-1] = '\0';
    ret = strlen(full);
  }
  else {
    /* very simple, even simplistic - since the stat above didn't
       work, we presume that the name given wasn't a full one, so we
       just slap the NETPERFDIR and path separator in front of the
       name and try again. */
    NETPERF_SNPRINTF(path,PATH_MAX,"%s%s%s",NETPERFDIR,NETPERF_PATH_SEP,name);
    path[PATH_MAX] = '\0';
    if (0 == NETPERF_STAT(path,&buf)) {
      strncpy(full,path,fulllen);
      full[fulllen-1] = '\0';
      ret = strlen(full);
    }
    else {
      ret = -1;
    }
  }
  return ret;
}

#ifdef HAVE_GETHRTIME

void
netperf_timestamp(hrtime_t *timestamp)
{
  *timestamp = gethrtime();
}

uint64_t
delta_nano(hrtime_t *begin, hrtime_t *end)
{
  int64_t nsecs;
  nsecs = (*end) - (*begin);
  return(nsecs);
}

int
delta_micro(hrtime_t *begin, hrtime_t *end)
{
  int64_t nsecs;
  nsecs = (*end) - (*begin);
  return(nsecs/1000);
}

int
delta_milli(hrtime_t *begin, hrtime_t *end)
{
  int64_t nsecs;
  nsecs = (*end) - (*begin);
  return(nsecs/1000000);
}


#elif defined HAVE_GETTIMEOFDAY

void
netperf_timestamp(struct timeval *timestamp)
{
  gettimeofday(timestamp,NULL);
}

 /* return the difference (in micro seconds) between two timeval */
 /* timestamps */
int
delta_micro(struct timeval *begin,struct timeval *end)

{

  int usecs, secs;

  if (end->tv_usec < begin->tv_usec) {
    /* borrow a second from the tv_sec */
    end->tv_usec += 1000000;
    end->tv_sec--;
  }
  usecs = end->tv_usec - begin->tv_usec;
  secs  = end->tv_sec - begin->tv_sec;

  usecs += (secs * 1000000);

  return(usecs);

}
 /* return the difference (in milliseconds) between two timeval
    timestamps */
int
delta_milli(struct timeval *begin,struct timeval *end)

{

  int usecs, secs;

  if (end->tv_usec < begin->tv_usec) {
    /* borrow a second from the tv_sec */
    end->tv_usec += 1000000;
    end->tv_sec--;
  }
  usecs = end->tv_usec - begin->tv_usec;
  secs  = end->tv_sec - begin->tv_sec;

  usecs += (secs * 1000000);

  return(usecs/1000);

}
#else

void
netperf_timestamp(struct timeval *timestamp)
{
  timestamp->tv_sec = 0;
  timestamp->tv_usec = 0;
}

 /* return the difference (in micro seconds) between two timeval */
 /* timestamps */
int
delta_micro(struct timeval *begin,struct timeval *end)

{

  return(1000000000);

}
 /* return the difference (in milliseconds) between two timeval
    timestamps */
int
delta_milli(struct timeval *begin,struct timeval *end)

{

  return(1000000000);

}

#endif /* HAVE_GETHRTIME */

/* This routine will return the two arguments to the calling routine. */
/* If the second argument is not specified, and there is no comma, */
/* then the value of the second argument will be the same as the */
/* value of the first. If there is a comma, then the value of the */
/* second argument will be the value of the second argument ;-) */
void
break_args(char *s, char *arg1, char *arg2)

{
  char *ns;

  ns = strchr(s,',');
  if (ns) {
    /* there was a comma arg2 should be the second arg*/
    *ns++ = '\0';
    while ((*arg2++ = *ns++) != '\0');
  }
  else {
    /* there was not a comma, we can use ns as a temp s */
    /* and arg2 should be the same value as arg1 */
    ns = s;
    while ((*arg2++ = *ns++) != '\0');
  };
  while ((*arg1++ = *s++) != '\0');
}

/* break_args_explicit

   this routine is somewhat like break_args in that it will separate a
   pair of comma-separated values.  however, if there is no comma,
   this version will not ass-u-me that arg2 should be the same as
   arg1. raj 2005-02-04 */
void
break_args_explicit(char *s, char *arg1, char *arg2)

{
  char *ns;

  ns = strchr(s,',');
  if (ns) {
    /* there was a comma arg2 should be the second arg*/
    *ns++ = '\0';
    while ((*arg2++ = *ns++) != '\0');
  }
  else {
    /* there was not a comma, so we should make sure that arg2 is \0
       lest something become confused. raj 2005-02-04 */
    *arg2 = '\0';
  };
  while ((*arg1++ = *s++) != '\0');

}

/* given a string with possible values for setting an address family,
   convert that into one of the AF_mumble values - AF_INET, AF_INET6,
   AF_UNSPEC as apropriate. the family_string is compared in a
   case-insensitive manner */

int
parse_address_family(char family_string[])
{

  char temp[10];  /* gotta love magic constants :) */


  strncpy(temp,family_string,10);

  if (debug) {
    fprintf(where,
	    "Attempting to parse address family from %s derived from %s\n",
	    temp,
	    family_string);
  }
#if defined(AF_INET6)
  if (strstr(temp,"6")) {
    return(AF_INET6);
  }
#endif
  if (strstr(temp,"inet") ||
      strstr(temp,"4")) {
    return(AF_INET);
  }
  if (strstr(temp,"unspec") ||
      strstr(temp,"0")) {
    return(AF_UNSPEC);
  }
  fprintf(where,
	  "WARNING! %s not recognized as an address family, using AF_UNPSEC\n",
	  family_string);
  fprintf(where,
	  "Are you sure netperf was configured for that address family?\n");
  fflush(where);
  return(AF_UNSPEC);
}

static void  
test_state_to_string(int value, char *string)
{
  char *state; 

  switch (value) {
  case TEST_PREINIT:
    state = "PRE ";
    break;
  case TEST_INIT:
    state = "INIT";
    break;
  case TEST_IDLE:
    state = "IDLE";
    break;
  case TEST_MEASURE:
    state = "MEAS";
    break;
  case TEST_LOADED:
    state = "LOAD";
    break;
  case TEST_ERROR:
    state = "ERR ";
    break;
  case TEST_DEAD:
    state = "DEAD";
    break;
  default:
    state = "ILLG";
    break;
  }
  strcpy(string,state);
}

void
report_test_status(test_t *test)
{
  char        current[8];
  char        requested[8];
  char        reported[8];

      
  test_state_to_string(test->state,     reported );
  test_state_to_string(test->new_state, current  );
  test_state_to_string(test->state_req, requested);

  fprintf(where,"%4s %4s %15s  %4s %4s %4s\n",
          test->server_id,test->id,test->test_name,
          reported, current, requested);
  fflush(where);
}

void
report_servers_test_status(server_t *server)
{
  int          ret;
  test_hash_t *h;
  test_t      *test;
  int          i;

  fprintf(where,"\n\n%4s %4s %15s  %4s %4s %4s\n",
          "srvr","tst","test_name","CURR","TEST","RQST");
  for (i = 0; i < TEST_HASH_BUCKETS; i ++) {
    h = &test_hash[i];
    NETPERF_MUTEX_LOCK(h->hash_lock);
        test = h->test;

    while (test != NULL) {
      if (!xmlStrcmp(test->server_id,server->id)) {
        report_test_status(test);
      }
      test = test->next;
    }
    NETPERF_MUTEX_UNLOCK(h->hash_lock);
  }
}


void
display_test_hash()
{
  int i;
  test_t * test;

  if (debug) {
    for (i=0;i < TEST_HASH_BUCKETS; i++) {
      test = test_hash[i].test;
      fprintf(where,"test_hash[%d]=%p\n",i,test_hash[i].test);
      while (test) {
        fprintf(where,"\ttest->id %s, test->state %d\n",
                test->id,test->state);
        test = test->next;
      }
      fflush(where);
    }
  }
}

char *
npe_to_str(int npe_error) {
  switch (npe_error) {
  case NPE_SUCCESS:
    return("NPE_SUCCESS");
    break;
  case  NPE_COMMANDED_TO_EXIT_NETPERF:
    return("NPE_COMMANDED_TO_EXIT_NETPERF");
    break;
  case NPE_TEST_SET_NOT_FOUND:
    return("NPE_TEST_SET_NOT_FOUND");
    break;
  case NPE_BAD_TEST_RANGE:
    return("NPE_BAD_TEST_RANGE");
    break;
  case NPE_BAD_TEST_ID:
    return(" NPE_BAD_TEST_ID");
    break;
  case NPE_SYS_STATS_DROPPED:
    return("NPE_SYS_STATS_DROPPED");
    break;
  case NPE_TEST_STATS_DROPPED:
    return("NPE_TEST_STATS_DROPPED");
    break;
  case NPE_TEST_NOT_FOUND:
    return("NPE_TEST_NOT_FOUND");
    break;
  case NPE_TEST_FOUND_IN_ERROR_STATE:
    return("NPE_TEST_FOUND_IN_ERROR_STATE");
    break;
  case NPE_TEST_INITIALIZED_FAILED:
    return("NPE_TEST_INITIALIZED_FAILED");
    break;
  case NPE_TEST_INIT_FAILED:
    return("NPE_TEST_INIT_FAILED");
    break;
  case NPE_INIT_TEST_XMLCOPYNODE_FAILED:
    return("NPE_INIT_TEST_XMLCOPYNODE_FAILED");
    break;
  case NPE_INIT_TEST_XMLNEWDOC_FAILED:
    return(" NPE_INIT_TEST_XMLNEWDOC_FAILED");
    break;
  case NPE_EMPTY_MSG:
    return("NPE_EMPTY_MSG");
    break;
  case NPE_UNEXPECTED_MSG:
    return("NPE_UNEXPECTED_MSG");
    break;
  case NPE_ALREADY_CONNECTED:
    return("NPE_ALREADY_CONNECTED");
    break;
  case NPE_BAD_VERSION:
    return("NPE_BAD_VERSION");
    break;
  case NPE_XMLCOPYNODE_FAILED:
    return("NPE_XMLCOPYNODE_FAILED");
    break;
  case NPE_PTHREAD_MUTEX_INIT_FAILED:
    return("NPE_PTHREAD_MUTEX_INIT_FAILED");
    break;
  case NPE_PTHREAD_RWLOCK_INIT_FAILED:
    return("NPE_PTHREAD_RWLOCK_INIT_FAILED");
    break;
  case NPE_PTHREAD_COND_WAIT_FAILED:
    return("NPE_PTHREAD_COND_WAIT_FAILED");
    break;
  case NPE_PTHREAD_DETACH_FAILED:
    return("NPE_PTHREAD_DETACH_FAILED");
    break;
  case NPE_PTHREAD_CREATE_FAILED:
    return("NPE_PTHREAD_CREATE_FAILED");
    break;
  case NPE_DEPENDENCY_NOT_PRESENT:
    return("NPE_DEPENDENCY_NOT_PRESENT");
    break;
  case NPE_DEPENDENCY_ERROR:
    return("NPE_DEPENDENCY_ERROR");
    break;
  case NPE_UNKNOWN_FUNCTION_TYPE:
    return("NPE_UNKNOWN_FUNCTION_TYPE");
    break;
  case NPE_FUNC_NAME_TOO_LONG:
    return("NPE_FUNC_NAME_TOO_LONG");
    break;
  case NPE_FUNC_NOT_FOUND:
    return("NPE_FUNC_NOT_FOUND");
    break;
  case NPE_LIBRARY_NOT_LOADED:
    return("NPE_LIBRARY_NOT_LOADED");
    break;
  case NPE_ADD_TO_EVENT_LIST_FAILED:
    return("NPE_ADD_TO_EVENT_LIST_FAILED");
    break;
  case NPE_CONNECT_FAILED:
    return("NPE_CONNECT_FAILED");
    break;
  case NPE_MALLOC_FAILED6:
    return("NPE_MALLOC_FAILED6");
    break;
  case NPE_MALLOC_FAILED5:
    return("NPE_MALLOC_FAILED5");
    break;
  case NPE_MALLOC_FAILED4:
    return("NPE_MALLOC_FAILED4");
    break;
  case NPE_MALLOC_FAILED3:
    return("NPE_MALLOC_FAILED3");
    break;
  case NPE_MALLOC_FAILED2:
    return("NPE_MALLOC_FAILED2");
    break;
  case NPE_MALLOC_FAILED1:
    return("NPE_MALLOC_FAILED1");
    break;
  case NPE_REMOTE_CLOSE:
    return("NPE_REMOTE_CLOSE");
    break;
  case NPE_SEND_VERSION_XMLNEWNODE_FAILED:
    return("NPE_SEND_VERSION_XMLNEWNODE_FAILED");
    break;
  case NPE_SEND_VERSION_XMLSETPROP_FAILED:
    return("NPE_SEND_VERSION_XMLSETPROP_FAILED");
    break;
  case NPE_SEND_CTL_MSG_XMLDOCDUMPMEMORY_FAILED:
    return("NPE_SEND_CTL_MSG_XMLDOCDUMPMEMORY_FAILED");
    break;
  case NPE_SEND_CTL_MSG_XMLCOPYNODE_FAILED:
    return("NPE_SEND_CTL_MSG_XMLCOPYNODE_FAILED");
    break;
  case NPE_SEND_CTL_MSG_XMLNEWNODE_FAILED:
    return("NPE_SEND_CTL_MSG_XMLNEWNODE_FAILED");
    break;
  case NPE_SEND_CTL_MSG_XMLNEWDTD_FAILED:
    return("NPE_SEND_CTL_MSG_XMLNEWDTD_FAILED");
    break;
  case NPE_SEND_CTL_MSG_XMLNEWDOC_FAILED:
    return("NPE_SEND_CTL_MSG_XMLNEWDOC_FAILED");
    break;
  case NPE_SEND_CTL_MSG_FAILURE:
    return("NPE_SEND_CTL_MSG_FAILURE");
    break;
  case NPE_SHASH_ADD_FAILED:
    return("NPE_SHASH_ADD_FAILED");
    break;
  case NPE_XMLPARSEMEMORY_ERROR:
    return("NPE_XMLPARSEMEMORY_ERROR");
    break;
  case NPE_NEG_MSG_BYTES:
    return("NPE_NEG_MSG_BYTES");
    break;
  case NPE_TIMEOUT:
    return("NPE_TIMEOUT");
    break;
  case NPE_SET_THREAD_LOCALITY_FAILED:
    return("NPE_SET_THREAD_LOCALITY_FAILED");
    break;
  default:
    return("Unmapped Error");
  }
}

int
add_test_to_hash(test_t *new_test)
{
  int hash_value;


  hash_value = TEST_HASH_VALUE(new_test->id);

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(test_hash[hash_value].hash_lock);

  new_test->next = test_hash[hash_value].test;
  test_hash[hash_value].test = new_test;

  NETPERF_MUTEX_UNLOCK(test_hash[hash_value].hash_lock);

  return(NPE_SUCCESS);
}

void
delete_test(const xmlChar *id)
{
  /* the removal of the test from any existing test set is done elsewhere */

  /* we presume that the id is of the form [a-zA-Z][0-9]+ and so will
     call atoi on id and mod that with the TEST_HASH_BUCKETS */

  int      hash_value;
  test_t  *test_pointer;
  test_t **prev_test;


  hash_value = TEST_HASH_VALUE(id);

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(test_hash[hash_value].hash_lock);

  prev_test    = &(test_hash[hash_value].test);
  test_pointer = test_hash[hash_value].test;
  while (test_pointer != NULL) {
    if (!xmlStrcmp(test_pointer->id,id)) {
      /* we have a match */
      *prev_test = test_pointer->next;
      free(test_pointer);
      break;
    }
    prev_test    = &(test_pointer->next);
    test_pointer = test_pointer->next;
  }

  NETPERF_MUTEX_UNLOCK(test_hash[hash_value].hash_lock);
}

test_t *
find_test_in_hash(const xmlChar *id)
{
  /* we presume that the id is of the form [a-zA-Z][0-9]+ and so will
     call atoi on id and mod that with the TEST_HASH_BUCKETS */

  int     hash_value;
  test_t *test_pointer;


  hash_value = TEST_HASH_VALUE(id);

  /* don't forget to add error checking one day */
  NETPERF_MUTEX_LOCK(test_hash[hash_value].hash_lock);

  test_pointer = test_hash[hash_value].test;
  while (test_pointer != NULL) {
    if (!xmlStrcmp(test_pointer->id,id)) {
      /* we have a match */
      break;
    }
    test_pointer = test_pointer->next;
  }

  NETPERF_MUTEX_UNLOCK(test_hash[hash_value].hash_lock);
  return(test_pointer);
}


void
dump_addrinfo(FILE *dumploc, struct addrinfo *info,
              xmlChar *host, xmlChar *port, int family)
{
  struct sockaddr *ai_addr;
  struct addrinfo *temp;
  temp=info;

  fprintf(dumploc, "getaddrinfo returned the following for host '%s' ", host);
  fprintf(dumploc, "port '%s' ", port);
  fprintf(dumploc, "family %d\n", family);
  while (temp) {
    fprintf(dumploc,
            "\tcannonical name: '%s'\n",temp->ai_canonname);
    fprintf(dumploc,
            "\tflags: %x family: %d: socktype: %d protocol %d addrlen %d\n",
            temp->ai_flags,
            temp->ai_family,
            temp->ai_socktype,
            temp->ai_protocol,
            temp->ai_addrlen);
    ai_addr = temp->ai_addr;
    if (ai_addr != NULL) {
      fprintf(dumploc,
              "\tsa_family: %d sadata: %d %d %d %d %d %d\n",
              ai_addr->sa_family,
              (u_char)ai_addr->sa_data[0],
              (u_char)ai_addr->sa_data[1],
              (u_char)ai_addr->sa_data[2],
              (u_char)ai_addr->sa_data[3],
              (u_char)ai_addr->sa_data[4],
              (u_char)ai_addr->sa_data[5]);
    }
    temp = temp->ai_next;
  }
  fflush(dumploc);
}

/* for libtool, we have an extra level of indirection to "normalize"
   the names of shared libraries, which means we have to open that
   file and look for the line with the "real" platform-specific shared
   library name.  that or use the liblt stuff, which does not appear
   to be thread-safe, or the documentation is very out of date. raj
   2005-10-10

   for those who are making their own test libraries, we do not want
   to dictate they use libtool, so if the "la" file actually ends in
   something other than ".la" we will ass-u-me that it is an actual
   library file and pass that back unscathed.  at some point this will
   also be the place where we follow LD_LIBRARY_PATH and the like so
   one does not have to fully-qualify the names. raj 2005-10-25

   this is not the prettiest code ever produced.  it could and
   probably should be a lot cleaner, but messing with pointers and
   strings has never been a great deal of fun.

   we probably need to check if "la" is already a fully qualified path
   name and simply pass it back if it is...  */

void
map_la_to_lib(xmlChar *la, char *lib) {

  FILE *la_file;
  int ret;
  char *last_paren;
  char s1[132];
  char name[NETPERF_MAX_TEST_LIBRARY_NAME];
  char *s2=name;
  char *dlname=NULL;
  char *libdir=NULL;
  int file_len;
  char *temp;
  char *ld_library_path = NULL;
  char full_path[PATH_MAX];
  char *last;
  char *s;
  struct stat buf;


  /* if we have no environment variables, this will not be
     overwritten */
  strncpy(full_path,(char *)la,PATH_MAX);

  /* for now, we will first look for NETPERF_LIBRARY_PATH, then
     LD_LIBRARY_PATH, then SHLIB_PATH. raj 2005-10-25 */
  temp = getenv("NETPERF_LIBRARY_PATH");
  if (NULL == temp) {
    temp = getenv("LD_LIBRARY_PATH");
    if (NULL == temp) {
      /* OK, there was no LD_LIBRARY_PATH, was there a SHLIB_PATH? I
	 wonder which if these should have precedence? */
      temp = getenv("SHLIB_PATH");
      if (NULL == temp) {
	/* OK, there were no paths at all, lets do our own internal
	   path, which one day really aught to be based on $(libdir)
	   from the make environment. raj 2006-02-22 */
	temp = ".:"LIBDIR;
      }
    }
  }

  if (NULL != temp) {
    ld_library_path = malloc(strlen(temp)+1);
  }

  if (NULL != ld_library_path) {
#ifdef WITH_GLIB
    gchar **tokens;
    int tok;
#endif
    /* OK, start trapsing down the path until we find a match */
    strcpy(ld_library_path,temp);
#ifdef WITH_GLIB
    tokens = g_strsplit(ld_library_path,":",15);
    for (tok = 0; tokens[tok] != NULL; tok++) {
      g_snprintf(full_path,PATH_MAX,"%s%s%s",tokens[tok],NETPERF_PATH_SEP,la);
      if (g_stat(full_path,&buf) == 0) {
	/* we have a winner, time to go */
	break;
      }
      else {
	/* put-back the original la file */
	strncpy(full_path,(char *)la,PATH_MAX);
      }
    }
    g_strfreev(tokens);
#else
    s = ld_library_path;
    while ((temp = strtok_r(s,":",&last)) != NULL) {
      s = NULL;
      snprintf(full_path,PATH_MAX,"%s%s%s",temp,NETPERF_PATH_SEP,la);
      if (stat(full_path,&buf) == 0) {
	/* we have a winner, time to go */
	break;
      }
      else {
	/* put-back the original la file */
	strncpy(full_path,(char *)la,PATH_MAX);
      }
    }
#endif
  }

  /* so, after all that, is it really a ".la" file or is it some other
     file. I wonder if we should ask that first and then ass-u-me that
     the dlopen will do the right thing instead of doing all the stuff
     with the environment variables - only looking at them if it is an
     "la" file?  raj 2005-10-26 */
  file_len = strlen(full_path);
  if ((full_path[file_len-1] != 'a') ||
      (full_path[file_len-2] != 'l') ||
      (full_path[file_len-3] != '.')) {
    /* we will ass-u-me it is an actual library file name */
    strcpy(lib,full_path);
  }
  else {
    la_file = fopen(full_path,"r");
    if (la_file != NULL) {
      /* time to start trapsing through the file */
      while ((ret = fscanf(la_file,
			   "%s",
			   s1)) != EOF) {
	if (ret == 1) {
	  /* algorithm depends on only one dlname and one libdir line in file */
	  ret = sscanf(s1,"dlname='%s",s2);
	  if (ret == 1) { 
	    /* we got our match. because %s matches until the next white space
	       the trailing single quote is in the string, which means we need
	       to nuke it */
	    last_paren = strchr(s2,'\'');
	    if (last_paren != NULL) {
	      *last_paren = '\0';
	    }
	    dlname = s2;
	    s2 = s2 + 1 + strlen(dlname);
	    if ((dlname != NULL) && (libdir != NULL)) {
	      break;
	    }
	  } else {
	    ret = sscanf(s1,"libdir='%s",s2);
	    if (ret == 1) {
	      /* we got our match. because %s matches until the next white space
		 the trailing single quote is in the string, which means we need
		 to nuke it */
	      last_paren = strchr(s2,'\'');
	      if (last_paren != NULL) {
		*last_paren = '\0';
	      }
	      libdir = s2;
	      s2 = s2 + 1 + strlen(libdir);
	      if ((dlname != NULL) && (libdir != NULL)) {
		break;
	      }
	    }
	  }
	}
	/* "digest" the rest of the line, adapted from some stuff
	   found in a web search */
	fscanf(la_file,"%*[^\n]");   /* Skip to the End of the Line */
	fscanf(la_file,"%*1[\n]");   /* Skip One Newline */
	strcpy(lib,"dlnamefound");
      }
      strcpy(lib,libdir);
      strcat(lib,NETPERF_PATH_SEP);
      strcat(lib,dlname);
    }
    else {
      strcpy(lib,"libfilenotfound");
    }
  }
  if (debug) {
    fprintf(where,"map_la_to_lib returning '%s' from '%s'\n",lib,(char *)la);
  }
}

GenReport
get_report_function(xmlNodePtr cmd)
{
  xmlChar  *la_file;
  char      lib_file[NETPERF_MAX_TEST_LIBRARY_NAME];
  xmlChar  *fname;
  GenReport func;

#ifdef WITH_GLIB
  gboolean ret;
  GModule  *lib_handle;
#else
  void     *lib_handle;
#endif


  /* first we do the xml stuff */
  la_file   = xmlGetProp(cmd, (const xmlChar *)"library");
  fname = xmlGetProp(cmd, (const xmlChar *)"function");

  map_la_to_lib(la_file,lib_file);

  if (debug) {
    fprintf(where,
            "trying to open library file '%s' via '%s'\n",
            lib_file,
            (char *)la_file);
    fflush(where);
  }
 
  /* now we do the dlopen/gmodule magic */

#ifdef WITH_GLIB
  lib_handle = g_module_open((const gchar *)lib_file,0);
#else
  lib_handle = dlopen((char *)lib_file, RTLD_NOW || RTLD_GLOBAL);
#endif
  if (debug) {
    fprintf(where,"open of library file '%s' returned %p\n",
            (char *)lib_file, lib_handle);
    if (lib_handle == NULL) {
#ifdef WITH_GLIB
      fprintf(where,"g_module_open error '%s'\n",g_module_error());
#else
      fprintf (where,"dlopen error '%s'\n",dlerror());
#endif 
    }
    fflush(where);
  }

#ifdef WITH_GLIB
  ret = g_module_symbol(lib_handle,fname,&func);
#else
  func  = (GenReport)dlsym(lib_handle,(char *)fname);
#endif
  if (debug) {
    fprintf(where,"symbol lookup of function '%s' returned %p\n",
            fname, func);
    if (func == NULL) {
#ifdef WITH_GLIB
      fprintf(where,"g_module_symbol error '%s'\n",g_module_error());
#else
      fprintf (where,"dlsym error '%s'\n",dlerror());
#endif
    }
    fflush(where);
  }

  return(func);
}

int
get_test_function(test_t *test, const xmlChar *func)
{
  int tmp = debug;
#ifdef WITH_GLIB
  GModule *lib_handle = test->library_handle;
  gboolean ret;
#else
  void    *lib_handle = test->library_handle;
#endif
  xmlChar *fname;
  void    *fptr = NULL;
  int      fnlen = 0;
  int      rc = NPE_FUNC_NAME_TOO_LONG;
  char     func_name[NETPERF_MAX_TEST_FUNCTION_NAME];


  if (debug) {
    fprintf(where,"get_test_func enter test %p func %s\n",test, func);
    fflush(where);
  }

  if (lib_handle == NULL) {
    xmlChar *la_file;
    char lib_file[NETPERF_MAX_TEST_LIBRARY_NAME];
    /* load the library for the test */
    la_file   = xmlGetProp(test->node,(const xmlChar *)"library");
    map_la_to_lib(la_file,lib_file);
    if (debug) {
      fprintf(where,
              "trying to open library file '%s' via '%s'\n",
              lib_file,
              (char *)la_file);
      fflush(where);
    }
#ifdef WITH_GLIB
    lib_handle = g_module_open((const gchar *)lib_file,0);
#else
    lib_handle = dlopen((char *)lib_file, RTLD_NOW || RTLD_GLOBAL);
#endif
    if (debug) {
      fprintf(where,"open of library file '%s' returned handle %p\n",
              (char *)lib_file, lib_handle);
      if (lib_handle == NULL) {
#ifdef WITH_GLIB
	fprintf(where,"g_module_open error '%s'\n",g_module_error());
#else
        fprintf(where,"dlopen error '%s'\n",dlerror());
#endif
      }
      fflush(where);
    }
    test->library_handle = lib_handle;
  }

  fname = xmlGetProp(test->node,func);
  if (!fname) {
    fnlen = 0;
  }
  else {
    fnlen = strlen((char *)fname);
  }
  if (debug) {
    if (fname) {
      fprintf(where,"func = '%s'  fname = '%s' fname[0] = %c fnlen = %d\n",
	      (char *)func, fname, fname[0], fnlen);
    }
    else {
      fprintf(where,"func = '%s'  fname = '' fname[0] = '' fnlen = %d\n",
	      (char *)func, fnlen);
    }
    fflush(where);
  }

  if (fnlen < NETPERF_MAX_TEST_FUNCTION_NAME) {
    if (fnlen) {
      strcpy(func_name,(char *)fname);
      rc = NPE_SUCCESS;
    } else {
      xmlChar *tname      = test->test_name;
      int      tnlen = strlen((char *)tname);

      strcpy(func_name,(char *)tname);
      if (debug) {
        fprintf(where,"func_name = '%s' tnlen = %d\n",
                (char *)func_name, tnlen);
        fflush(where);
      }

      if (!xmlStrcmp(func,(const xmlChar *)"test_clear")) {
        if (strlen("_clear_stats") < (size_t)(NETPERF_MAX_TEST_FUNCTION_NAME-tnlen)) {
          strcat(func_name,"_clear_stats");
          rc = NPE_SUCCESS;
        } else {
          rc = NPE_FUNC_NAME_TOO_LONG;
        }
      } else if (!xmlStrcmp(func,(const xmlChar *)"test_stats")) {
        if (strlen("_get_stats") < (size_t)(NETPERF_MAX_TEST_FUNCTION_NAME-tnlen)) {
          strcat(func_name,"_get_stats");
          rc = NPE_SUCCESS;
        } else {
          rc = NPE_FUNC_NAME_TOO_LONG;
        }
      } else if (!xmlStrcmp(func,(const xmlChar *)"test_decode")) {
        if (strlen("_decode_stats") < (size_t)(NETPERF_MAX_TEST_FUNCTION_NAME-tnlen)) {
          strcat(func_name,"_decode_stats");
          rc = NPE_SUCCESS;
        } else {
          rc = NPE_FUNC_NAME_TOO_LONG;
        }
      } else {
        rc = NPE_UNKNOWN_FUNCTION_TYPE;
      }
    }
    if (debug) {
      fprintf(where,"func_name = '%s'\n", (char *)func_name);
      fflush(where);
    }
    if (rc == NPE_SUCCESS) {
#ifdef WITH_GLIB
      ret = g_module_symbol(lib_handle,func_name,&fptr);
#else
      fptr = dlsym(lib_handle,func_name);
#endif
      if (debug) {
        fprintf(where,"symbol lookup of func_name '%s' returned %p\n",
                func_name, fptr);
        if (fptr == NULL) {
#ifdef WITH_GLIB
	  fprintf(where,"g_module_symbol error '%s'\n",g_module_error());
#else
          fprintf (where,"dlsym error '%s'\n",dlerror());
#endif
        }
        fflush(where);
      }
      if (fptr == NULL) {
        if (lib_handle) {
          rc = NPE_FUNC_NOT_FOUND;
        } else {
          rc = NPE_LIBRARY_NOT_LOADED;
        }
      }
    }
  } else {
    rc = NPE_FUNC_NAME_TOO_LONG;
  }

  if (!xmlStrcmp(func,(const xmlChar *)"test_name")) {
    test->test_name    = fname;
    test->test_func    = (TestFunc)fptr;
  } else if (!xmlStrcmp(func,(const xmlChar *)"test_clear")) {
    test->test_clear   = (TestClear)fptr;
    xmlFree(fname);
  } else if (!xmlStrcmp(func,(const xmlChar *)"test_stats")) {
    test->test_stats   = (TestStats)fptr;
    xmlFree(fname);
  } else if (!xmlStrcmp(func,(const xmlChar *)"test_decode")) {
    test->test_decode  = (TestDecode)fptr;
    xmlFree(fname);
  } else {
    rc = NPE_UNKNOWN_FUNCTION_TYPE;
    xmlFree(fname);
  }

  if (rc != NPE_SUCCESS) {
    if (debug) {
      fprintf(where,
              "get_test_function: error %d occured getting function %s\n",
              rc,func_name);
      fflush(where);
    }
  }
  debug = tmp;
  return(rc);
}


/* I would have used the NETPERF_THREAD_T abstraction, but that would
   make netlib.h dependent on netperf.h and I'm not sure I want to do
   that. raj 2006-03-02 */
int
#ifdef WITH_GLIB
launch_thread(GThread **tid, void *(*start_routine)(void *), void *data)
#else
launch_thread(pthread_t *tid, void *(*start_routine)(void *), void *data)
#endif

{
  int rc;

#ifdef WITH_GLIB
  NETPERF_THREAD_T temp_tid;

  temp_tid = g_thread_create(start_routine,data,FALSE,NULL);

  if (NULL != temp_tid) {
    rc = 0;
    *tid = temp_tid;
  }
  else {
    rc = -1;
  }
#else

  NETPERF_THREAD_T temp_tid;
  rc = pthread_create(&temp_tid, (pthread_attr_t *)NULL, start_routine, data);
  if (rc != 0) {
    if (debug) {
      fprintf(where,"launch_thread: pthread_create failed with %d\n",rc);
      fflush(where);
    }
    rc = NPE_PTHREAD_CREATE_FAILED;
  } else {
    /* pthread_create succeeded detach thread so we don't need to join */
    *tid = temp_tid;
    if (debug) {
      fprintf(where,"launch_thread: pthread_create succeeded id = %d\n",*tid);
      fflush(where);
    }
    rc = pthread_detach(temp_tid);
    if (rc != 0) {
      if (debug) {
        fprintf(where,"launch_thread: pthread_detach failed %d\n",rc);
        fflush(where);
      }
      rc = NPE_PTHREAD_DETACH_FAILED;
    } else {
      if (debug) {
        fprintf(where,"launch_thread: pthread_detach succeed %d\n",rc);
        fflush(where);
      }
      rc = NPE_SUCCESS;
    }
  }
#endif
  return(rc);
}



int
strtofam(xmlChar *familystr)
{

  if (debug) {
    fprintf(where, "strtofam called with %s\n", familystr);
    fflush(where);
  }

  if (!xmlStrcmp(familystr,(const xmlChar *)"AF_INET")) {
    return(AF_INET);
  } else if (!xmlStrcmp(familystr,(const xmlChar *)"AF_UNSPEC")) {
    return(AF_UNSPEC);
#ifdef AF_INET6
  } else if (!xmlStrcmp(familystr,(const xmlChar *)"AF_INET6")) {
    return(AF_INET6);
#endif /* AF_INET6 */
  } else {
    /* we should never get here if the validator is doing its thing */
    fprintf(where, "strtofam: unknown address family %s\n",familystr);
    fflush(where);
    return(-1);
  }
}


/*
  establish_listen()

Set-up a listen socket for netserver or a test thread so netperf or dependent
test thread can connect or communicate with it.

*/

int
establish_listen(char *hostname, char *service, int af, netperf_socklen_t *addrlenp)
{
  SOCKET sockfd;
  int error;
  int count;
  int len = *addrlenp;
  int one = 1;
  struct addrinfo hints, *res, *res_temp;

  if (debug) {
    fprintf(where,
	    "establish_listen: host '%s' service '%s' af %d socklen %d\n",
	    hostname,
	    service,
	    af,
	    len);
    fflush(where);
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = af;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags    = AI_PASSIVE;

  count = 0;
  do {
    error = getaddrinfo(hostname,
                           service,
                           &hints,
                           &res);
    count += 1;
    if (error == EAI_AGAIN) {
      if (debug) {
        fprintf(where,"Sleeping on getaddrinfo EAI_AGAIN\n");
        fflush(where);
      }
#ifdef WITH_GLIB
      g_usleep(1000);
#else
      sleep(1);
#endif
    }
  } while ((error == EAI_AGAIN) && (count <= 5));

  if (error) {
    fprintf(where,
            "establish_listen: could not resolve host '%s' service '%s'\n",
            hostname,service);
    fprintf(where,"\tgetaddrinfo returned %d %s\n",
            error,gai_strerror(error));
    fflush(where);
    return(-1);
  }


  if (debug) {
    dump_addrinfo(where, res, (xmlChar *)hostname,
                  (xmlChar *)service, AF_UNSPEC);
  }

  res_temp = res;

  do {

    sockfd = socket(res_temp->ai_family,
                    res_temp->ai_socktype,
                    res_temp->ai_protocol);
    if (sockfd == INVALID_SOCKET) {
      if (debug) {
        fprintf(where,"establish_listen: socket error trying next one\n");
        fflush(where);
      }
      continue;
    }
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one)) == 
	SOCKET_ERROR) {
      fprintf(where,"establish_listen: SO_REUSEADDR failed\n");
      fflush(where);
    }
    if (bind(sockfd, res_temp->ai_addr, res_temp->ai_addrlen) == 0) {
      break;
    }
    fprintf(where,"establish_listen: bind error close and try next\n");
    fflush(where);
    close(sockfd);
  } while ( (res_temp = res_temp->ai_next) != NULL );

  if (res_temp == NULL) {
    fprintf(where,"establish_listen: allocate server socket failed\n");
    fflush(where);
    sockfd = -1;
  } else if (listen (sockfd,20) == -1) {
    fprintf(where,"establish_listen: setting the listen backlog failed\n");
    fflush(where);
    close(sockfd);
    sockfd = -1;
  } else {
    if (addrlenp) *addrlenp = res_temp->ai_addrlen;
  }

  freeaddrinfo(res);

  return (sockfd);

}



/*
  establish_control()

set-up the control connection between netperf and the netserver so we
can actually run some tests. if we cannot establish the control
connection, that may or may not be a good thing, so we will let the
caller decide what to do.

to assist with pesky end-to-end-unfriendly things like firewalls, we
allow the caller to specify both the remote hostname and port, and the
local addressing info.  i believe that in theory it is possible to
have an IPv4 endpoint and an IPv6 endpoint communicate with one
another, but for the time being, we are only going to take-in one
requested address family parameter. this means that the only way
(iirc) that we might get a mixed-mode connection would be if the
address family is specified as AF_UNSPEC, and getaddrinfo() returns
different families for the local and server names.

the "names" can also be IP addresses in ASCII string form.

raj 2003-02-27 */

int
establish_control(xmlChar *hostname,
                  xmlChar *port,
                  int      remfam,
                  xmlChar *localhost,
                  xmlChar *localport,
                  int      locfam)
{
  int not_connected;
  int control_sock;
  int count;
  int error;

  struct addrinfo   hints;
  struct addrinfo  *local_res;
  struct addrinfo  *remote_res;
  struct addrinfo  *local_res_temp;
  struct addrinfo  *remote_res_temp;

  if (debug) {
    fprintf(where,
            "establish_control called with host '%s' port '%s' remfam %d\n",
            hostname,
            port,
            remfam);
    fprintf(where,
            "\t\tlocal '%s' port '%s' locfam %d\n",
            localhost,
            localport,
            locfam);
    fflush(where);
  }

  /* first, we do the remote */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = remfam;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = 0;
  count = 0;
  do {
    error = getaddrinfo((char *)hostname,
                        (char *)port,
                        &hints,
                        &remote_res);
    count += 1;
    if (error == EAI_AGAIN) {
      if (debug) {
        fprintf(where,"Sleeping on getaddrinfo EAI_AGAIN\n");
        fflush(where);
      }
#ifdef WITH_GLIB
      g_usleep(1000);
#else
      sleep(1);
#endif
    }
  } while ((error == EAI_AGAIN) && (count <= 5));

  if (error) {
    printf("establish control: could not resolve remote '%s' port '%s' af %d",
           hostname,
           port,
           remfam);
    printf("\n\tgetaddrinfo returned %d %s\n",
           error,
           gai_strerror(error));
    return(-1);
  }

  if (debug) {
    dump_addrinfo(where, remote_res, hostname, port, remfam);
  }

  /* now we do the local */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = locfam;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;
  count = 0;
  do {
    count += 1;
    error = getaddrinfo((char *)localhost,
                           (char *)localport,
                           &hints,
                           &local_res);
    if (error == EAI_AGAIN) {
      if (debug) {
        fprintf(where,
                "Sleeping on getaddrinfo(%s,%s) EAI_AGAIN count %d \n",
                localhost,
                localport,
                count);
        fflush(where);
      }
#ifdef WITH_GLIB
      g_usleep(1000);
#else
      sleep(1);
#endif
    }
  } while ((error == EAI_AGAIN) && (count <= 5));

  if (error) {
    printf("establish control: could not resolve local '%s' port '%s' af %d",
           localhost,
           localport,
           locfam);
    printf("\n\tgetaddrinfo returned %d %s\n",
           error,
           gai_strerror(error));
    return(-1);
  }

  if (debug) {
    dump_addrinfo(where, local_res, localhost, localport, locfam);
  }

  not_connected = 1;
  local_res_temp = local_res;
  remote_res_temp = remote_res;
  /* we want to loop through all the possibilities. looping on the
     local addresses will be handled within the while loop.  I suppose
     these is some more "C-expert" way to code this, but it has not
     lept to mind just yet :)  raj 2003-02024 */

  while (remote_res_temp != NULL) {

    /* I am guessing that we should use the address family of the
       local endpoint, and we will not worry about mixed family types
       - presumeably the stack or other transition mechanisms will be
       able to deal with that for us. famous last words :)  raj 2003-02-26 */
    control_sock = socket(local_res_temp->ai_family,
                          SOCK_STREAM,
                          0);
    if (control_sock < 0) {
      /* at some point we'll need a more generic "display error"
         message for when/if we use GUIs and the like. unlike a bind
         or connect failure, failure to allocate a socket is
         "immediately fatal" and so we return to the caller. raj 2003-02-24 */
      if (debug) {
        perror("establish_control: unable to allocate control socket");
      }
      return(-1);
    }

    if (connect(control_sock,
                remote_res_temp->ai_addr,
                remote_res_temp->ai_addrlen) == 0) {
      /* we have successfully connected to the remote netserver */
      if (debug) {
        fprintf(where,
		"successful connection to remote netserver at %s and %s\n",
		hostname,
		port);
	fflush(where);
      }
      not_connected = 0;
      /* this should get us out of the while loop */
      break;
    } else {
      /* the connect call failed */
      if (debug) {
        fprintf(where, "establish_control: connect failed, errno %d\n",errno);
        fprintf(where, "    trying next address combination\n");
        fflush(where);
      }
    }

    if ((local_res_temp = local_res_temp->ai_next) == NULL) {
      /* wrap the local and move to the next server, don't forget to
         close the current control socket. raj 2003-02-24 */
      local_res_temp = local_res;
      /* the outer while conditions will deal with the case when we
         get to the end of all the possible remote addresses. */
      remote_res_temp = remote_res_temp->ai_next;
      /* it is simplest here to just close the control sock. since
         this is not a performance critical section of code, we
         don't worry about overheads for socket allocation or
         close. raj 2003-02-24 */
    }
    close(control_sock);
  }

  /* we no longer need the addrinfo stuff */
  freeaddrinfo(local_res);
  freeaddrinfo(remote_res);

  /* so, we are either connected or not */
  if (not_connected) {
    fprintf(where, "establish control: are you sure there is a netserver listening on %s at port %s?\n",hostname,port);
    fflush(where);
    return(-1);
  }
  /* at this point, we are connected.  we probably want some sort of
     version check with the remote at some point. raj 2003-02-24 */
  return(control_sock);
}


void
netlib_init()
{
}


int32_t
recv_control_message(int control_sock, xmlDocPtr *message)
{
  int loc_debug = 0;
  int32_t bytes_recvd = 0,
          bytes_left,
          counter;

  uint32_t message_len;
  char *read_ptr = NULL;
  char *message_base = NULL;

  struct pollfd fds;

  int timeout = 15000;


  /* one of these days, we probably aught to make sure that what
     message points to is NULL... but only as a debug assert... raj
     2003-03-05 */

  /* first, read-in the control header. initially, this is really
     simple - a four byte message length in network byte order.  if we
     were really concerned about speed and/or getting multiple
     messages in a single recv call, we would recv more than four
     bytes here */

  bytes_left = sizeof(uint32_t);
  read_ptr = (char *)&message_len;
  while (bytes_left > 0) {
    /* precede every recv with a poll() call, so there can be little
       chance of our ever getting hung-up in this routine. raj
       2003-02-26 */
    fds.fd = control_sock;
    fds.events = POLLIN;
    fds.revents = 0;

    /* poll had better return one, or there was either a problem or
       a timeout... */

    if ((counter = poll(&fds, 1, timeout)) != 1) {
      if (debug) {
        fprintf(where,
                "recv_control_message: poll error or timeout. errno %d counter %d\n",
                errno,
                counter);
        fflush(where);
      }
      return(NPE_TIMEOUT);
    }

    bytes_recvd = recv(control_sock,
                       read_ptr,
                       bytes_left,
                       0);
    if (bytes_recvd < 0) {
      fprintf(where,
              "Unexpected byte count on control message of %d errno %d\n",
              bytes_recvd,
              errno);
      fflush(where);
      return(NPE_NEG_MSG_BYTES);
    }
    else if (bytes_recvd == 0) {
      /* eeew, a goto :) */
      goto remote_close;
    }
    /* we got some data, decrement the bytes remaining and bump our
       pointer */
    bytes_left -= bytes_recvd;
    read_ptr += bytes_recvd;
  }

  bytes_left = ntohl(message_len);
  /* might as well make message_len native too, just give it
     bytes_left. raj 2005-10-26 */
  message_len = bytes_left;

  /* at some point we probably need to sanity check that we aren't
     being asked for a massive message size, but for now, we will
     simply check the return value on the malloc() call :) raj
     2003-03-06 */
  if ((message_base = malloc(bytes_left+1)) == NULL) {
    fprintf(where,
            "recv_control_messsage: unable to allocate %u bytes for inbound message\n",
            bytes_left);
    fflush(where);
    return(NPE_MALLOC_FAILED1);
  }
  memset(message_base,'\0',bytes_left+1);  /* cover fprintf problem */

  read_ptr = message_base;

  while (bytes_left > 0) {
    /* precede every recv with a poll() call, so there can be little
       chance of our ever getting hung-up in this routine. raj 2003-02-26 */
    fds.fd = control_sock;
    fds.events = POLLIN;
    fds.revents = 0;

    /* poll had better return one, or there was either a problem or
       a timeout... */

    if ((counter = poll(&fds, 1, 15000)) != 1) {
      if (debug) {
        fprintf(where,
                "recv_control_message: poll error or timeout. errno %d counter %d\n",
                errno,
                counter);
        fflush(where);
      }
      return(NPE_TIMEOUT);
    }
    bytes_recvd = recv(control_sock,
                       read_ptr,
                       bytes_left,
                       0);
    if (bytes_recvd < 0) {
      if (debug) {
        fprintf(where,
                "Unexpected byte count on control message of %d errno %d\n",
                bytes_recvd,
                errno);
        fflush(where);
      }
      return(NPE_NEG_MSG_BYTES);
    }
    else if (bytes_recvd == 0) {
      /* eeew, another goto !) */
      goto remote_close;
    }
    /* we got some data, decrement the bytes remaining and bump our
       pointer */
    bytes_left -= bytes_recvd;
    read_ptr += bytes_recvd;
  }

  if (debug || loc_debug) {
    fprintf(where,
            "Just received %d byte message from sock %d\n",
            message_len,
            control_sock);
    fprintf(where,"|%*s|\n", message_len+1, message_base);
    fflush(where);
  }

  /* now that we have the message, time to parse it. */
  if ((*message = xmlParseMemory(message_base,message_len)) != NULL) {
    if (debug) {
      fprintf(where, "recv_control_message: xmlParseMemory returned %p\n",
              *message);
      fflush(where);
    }
    return(message_len);
  } else {
    if (debug) {
      fprintf(where, 
	      "recv_control_message: xmlParseMemory of message %p len %d  failed\n",
	      message_base,message_len);
      fflush(where);
    }
    free(message_base);
    return(NPE_XMLPARSEMEMORY_ERROR);
  }

remote_close:
  if (debug) {
    fprintf(where,
            "recv_control_message: remote requested shutdown of control\n");
    fflush(where);
  }

  if (message_base) {
    free(message_base);
  }

  /* just like read/recv, a zero byte return means that the remote
     closed the connection.  raj 2003-03-05 */
  return(0);

}


/* send_control_message expects to be called with a socket, and an
   xmlNodePtr containing the element to send in the message. It is also
   given the netserver nid to which the message is addressed. It will
   then use these to construct an XML document that encapsulates the message
   and will send that on its way.  At some point, we may remove the socket
   parm and have that looked-up to keep things a bit more "abstracted".
   raj 2003-02-28  sgb 2003-08-22 */
int
send_control_message(const int control_sock,
                     xmlNodePtr body,
                     xmlChar *nid,
                     const xmlChar *fromnid)
{
  int  rc = NPE_SUCCESS;

  int32_t length;
  struct iovec hdrtrl[2];     /* used for the sendmsg call */
  struct msghdr message_hdr;  /* used for the sendmsg call */

  xmlDocPtr doc;
  xmlDtdPtr dtd;
  xmlNodePtr message_header;
  xmlNodePtr new_node;
  char *control_message;
  int  control_message_len;


  if (debug) {
    fprintf(where,
            "send_control_message: called with sock %d and message node at %p",
            control_sock,
            body);
    fprintf(where,
            " type %s destined for nid %s from nid %s\n",
            body->name,
            nid,
            fromnid);
    fflush(where);
  }

  if ((doc = xmlNewDoc((xmlChar *)"1.0")) != NULL) {
    /* zippity do dah */
    doc->standalone = 0;
    dtd = xmlCreateIntSubset(doc,(xmlChar *)"message",NULL,NETPERF_DTD_FILE);
    if (dtd != NULL) {
      if (((message_header = xmlNewNode(NULL,(xmlChar *)"message")) != NULL) &&
          (xmlSetProp(message_header,(xmlChar *)"tonid",nid) != NULL) &&
          (xmlSetProp(message_header,(xmlChar *)"fromnid",fromnid) != NULL)) {
        /* zippity ay */
        xmlDocSetRootElement(doc,message_header);
        /* it certainly would be nice to not have to do this with two
           calls... raj 2003-02-28 */
        if (((new_node = xmlDocCopyNode(body,doc,1)) != NULL) &&
            (xmlAddChild(message_header, new_node) != NULL)) {
          /* my oh my */
          xmlDocDumpMemory(doc,
                           (xmlChar **)&control_message,
                           &control_message_len);
          if (control_message_len > 0) {
            /* what a wonderful day */
            /* the message length send via the network does not include
               the length itself... raj 2003-02-27 */
            length = htonl(strlen(control_message));

            /* offset zero is the message length, offset one is the
               message itself */
            hdrtrl[0].iov_len  = sizeof(length);
            hdrtrl[0].iov_base = (void *)&length;
            hdrtrl[1].iov_len  = control_message_len;
            hdrtrl[1].iov_base = control_message;

            /* since we are only interested in the msg_iov, and
               msg_iovlen, and since we would set all the other fields,
               regardless of "flavor" to a value of 0 (or NULL, which we
               ass-u-me to == 0), we will just use a memset and then not
               have to worry about #ifdef'ing for the various flavors of
               struct msghdr. raj 2003-02-26 */

            memset(&message_hdr,0,sizeof(struct msghdr));
            message_hdr.msg_iov = hdrtrl;
            message_hdr.msg_iovlen = 2;

            rc = sendmsg(control_sock,
                         &message_hdr,
                         0);
            if (debug) {
              /* first display the header */
              fprintf(where, "Just sent a %d byte message\n",
                      control_message_len);
              fprintf(where, "|%*s|\n",control_message_len,control_message);
              fflush(where);
            }
            if (rc == (control_message_len+sizeof(length))) {
              rc = NPE_SUCCESS;
            } else {
              rc = NPE_SEND_CTL_MSG_FAILURE;
            }
          } else {
            rc = NPE_SEND_CTL_MSG_XMLDOCDUMPMEMORY_FAILED;
          }
        } else {
          rc = NPE_SEND_CTL_MSG_XMLCOPYNODE_FAILED;
        }
      } else {
        rc = NPE_SEND_CTL_MSG_XMLNEWNODE_FAILED;
      }
    } else {
      rc = NPE_SEND_CTL_MSG_XMLNEWDTD_FAILED;
    }
  } else {
    rc = NPE_SEND_CTL_MSG_XMLNEWDOC_FAILED;
  }
  return(rc);
}


void
report_server_error(server_t *server)
{
  int i;


  i = server->err_rc - NPE_MIN_ERROR_NUM;
  fprintf(where,
          "server %s entered error state %d from %s error code %d %s\n",
	  server->id,
	  server->state_req,
	  server->err_fn,
	  server->err_rc,
          NP_ERROR_NAMES[i]);
  fflush(where);
}

const char *
netperf_error_name(int rc)
{
  int i;


  i = rc - NPE_MIN_ERROR_NUM;
  return(NP_ERROR_NAMES[i]);
}
