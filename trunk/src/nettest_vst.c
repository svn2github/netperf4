/*        Copyright (C) 2005 Hewlett-Packard Company */

 /* This file contains the test-specific definitions for netperf4's */
 /* Hewlett-Packard Company special variable sized data tests */

#ifndef lint
char    nettest_id[]="\
@(#)nettest_vst.c (c) Copyright 2005 Hewlett-Packard Co. $Id$";
#else
#define DIRTY
#define WANT_HISTOGRAM
#define INTERVALS
#endif /* lint */

#ifdef DIRTY
#define MAKE_DIRTY(mydata,ring)  /* DIRTY is not currently supported */
#else
#define MAKE_DIRTY(mydata,ring)  /* DIRTY is not currently supported */
#endif



/****************************************************************/
/*                                                              */
/*      nettest_vst.c                                           */
/*                                                              */
/*      scan_vst_args()                                         */
/*                                                              */
/*      the actual test routines...                             */
/*                                                              */
/*      send_vst_rr()           perform a tcp req/rsp test      */
/*                              with variable sized req/rsp     */
/*      recv_vst_rr()           catch a tcp req/rsp test        */
/*                              with variable sized req/rsp     */
/*                                                              */
/****************************************************************/



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_VALUES_H
#include <values.h>
#endif

#ifdef HAVE_FLOAT_H
#include <float.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIM
#include <time.h>
#endif
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include "netperf.h"

#include "nettest_vst.h"


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
  int   curr_state;
  int   state;
  int   valid = 0;
  char *state_name;
  char  error_msg[1024];
  
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
        test->new_state = state;
      }
      else {
        sprintf(error_msg,"bad state transition from %s state",state_name);
        report_test_failure( test,
                             (char *)__func__,
                             VSTE_REQUESTED_STATE_INVALID,
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
      free_routine(test);
      free(GET_TEST_DATA(test));
      SET_TEST_DATA(test, NULL);
      test->new_state = TEST_DEAD;
    }
  }
}

#ifdef OFF
/* the following lines are a template for any test
   just copy the lines for generic_test change
   the procedure name and write you own TEST_SPECIFC_XXX
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
    case TEST_LOADED:
      new_state = TEST_SPECIFIC_LOAD(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test,TEST_SPECIFIC_FREE);
}
 
#endif /* OFF end of generic_test example code  sgb  2005-10-26 */


static void
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
            "\tflags: %d family: %d: socktype: %d protocol %d addrlen %d\n",
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


static int
strtofam(xmlChar *familystr)
{
  if (!xmlStrcmp(familystr,(const xmlChar *)"AF_INET")) {
    return(AF_INET);
  } else if (!xmlStrcmp(familystr,(const xmlChar *)"AF_UNSPEC")) {
    return(AF_UNSPEC);
#ifdef AF_INET6
  } else if (!xmlStrcmp(familystr,(const xmlChar *)"AF_INET6")) {
    return(AF_INET6);
#endif /* AF_INET6 */
  }
  else {
    /* we should never get here if the validator is doing its thing */
    return(-1);
  }
}

static void
get_dependency_data(test_t *test, int type, int protocol)
{
  vst_data_t *my_data = GET_TEST_DATA(test);

  xmlChar *string;
  xmlChar *remotehost;
  xmlChar *remoteport;
  int      remotefam;

  int      count;
  int      error;

  struct addrinfo  hints;
  struct addrinfo *remote_ai;
  
  /* still need to finish */
  /* now get and initialize the remote addressing info */
  string      =  xmlGetProp(test->dependency_data,(const xmlChar *)"family");
  remotefam   = strtofam(string);
  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = remotefam;
  hints.ai_socktype = type;
  hints.ai_protocol = protocol;
  hints.ai_flags    = 0;

  remoteport = xmlGetProp(test->dependency_data,(const xmlChar *)"remote_port");
  remotehost = xmlGetProp(test->dependency_data,(const xmlChar *)"remote_host");
  count = 0;
  error = EAI_AGAIN;
  do {
    error = getaddrinfo( (char *)remotehost, (char *)remoteport,
                            &hints, &remote_ai);
    count += 1;
    if (error == EAI_AGAIN) {
      if (test->debug) {
        fprintf(test->where,"Sleeping on getaddrinfo EAI_AGAIN\n");
        fflush(test->where);
      }
      sleep(1);
    }
  } while ((error == EAI_AGAIN) && (count <= 5));
    
  if (test->debug) {
    dump_addrinfo(test->where, remote_ai, remotehost, remoteport, remotefam);
  }

  if (!error) {
    my_data->remaddr = remote_ai;
  }
  else {
    if (test->debug) {
      fprintf(test->where,"%s: getaddrinfo returned %d %s\n",
              __func__, error, gai_strerror(error));
      fflush(test->where);
    }
    report_test_failure(test,
                        (char *)__func__,
                        VSTE_GETADDRINFO_ERROR,
                        gai_strerror(error));
  }
}


static void
set_dependent_data(test)
  test_t *test;
{
  
  vst_data_t  *my_data = GET_TEST_DATA(test);
  xmlChar *family;
  char port[8];
  char host[32];

  xmlNodePtr dep_data;

  switch (my_data->locaddr->ai_addr->sa_family) {
#ifdef AF_INET6
  case AF_INET6:
    family = (xmlChar *)"AF_INET6";
    break;
#endif
  case AF_INET:
    family = (xmlChar *)"AF_INET";
    break;
  default:
    family = (xmlChar *)"AF_UNSPEC";
    break;
  }

  getnameinfo(my_data->locaddr->ai_addr, my_data->locaddr->ai_addrlen,
                 host, sizeof(host), port, sizeof(port),
                 NI_NUMERICHOST | NI_NUMERICSERV);

  if ((dep_data = xmlNewNode(NULL,(xmlChar *)"dependency_data")) != NULL) {
   /* set the properties of the dependency data sbg 2004-06-08 */
    if ((xmlSetProp(dep_data,(const xmlChar *)"family",family)    != NULL) &&
        (xmlSetProp(dep_data,(const xmlChar *)"remote_port",
                    (xmlChar *)port) != NULL) &&
        (xmlSetProp(dep_data,(const xmlChar *)"remote_host",
                    (xmlChar *)host) != NULL))  {
    test->dependent_data = dep_data;
    }
    else {
      report_test_failure(test,
                          (char *)__func__,
                          VSTE_XMLSETPROP_ERROR,
                          "error setting properties for dependency data");
    }
  }
  else {
    report_test_failure(test,
                        (char *)__func__,
                        VSTE_XMLNEWNODE_ERROR,
                        "error getting new node for dependency data");
  }
}


unsigned int
convert(string,units)
     unsigned char *string;
     unsigned char *units;
{
  unsigned int base;
  base = atoi((char *)string);
  if (strstr((char *)units,"B")) {
    base *= 1;
  }
  if (strstr((char *)units,"KB")) {
    base *= 1024;
  }
  if (strstr((char *)units,"MB")) {
    base *= (1024 * 1024);
  }
  if (strstr((char *)units,"GB")) {
    base *= (1024 * 1024 * 1024);
  }
  if (strstr((char *)units,"kB")) {
    base *= (1000);
  }
  if (strstr((char *)units,"mB")) {
    base *= (1000 * 1000);
  }
  if (strstr((char *)units,"gB")) {
    base *= (1000 * 1000 * 1000);
  }
  return(base);
}


 /* this routine will allocates a circular list of fixed buffers for  */
 /* send and receive operations. each of these buffers will be offset */
 /* as per the users request and then align will be adjusted to be a  */
 /* sizeof(int) or a multiple of size of int before buffer alignment. */
 /* the circumference of this ring will be controlled by the setting  */
 /* of send_width. the buffers will be filled with data from the file */
 /* specified in fill_file. if fill_file is an empty string, then     */
 /* buffers will not be filled with any particular data.              */

static void 
allocate_fixed_buffers(test_t *test)
{
  vst_data_t  *my_data;
  vst_ring_ptr temp_link;
  vst_ring_ptr prev_link;
  int          width;
  int          send_malloc_size;
  int          send_size;
  int          send_align;
  int          send_offset;
  int          recv_malloc_size;
  int          recv_size;
  int          recv_align;
  int          recv_offset;
  int         *send_buf;
  int          i;
  int          send = 0;

  my_data     = GET_TEST_DATA(test);

  if (!xmlStrcmp(test->test_name,(const xmlChar *)"send_vst_rr")) {
    send = 1;
  }

  if (send) {
    width       = my_data->send_width;
    send_size   = my_data->req_size;
    send_align  = my_data->send_align;
    send_offset = my_data->send_offset;
    recv_size   = my_data->rsp_size;
    recv_align  = my_data->recv_align;
    recv_offset = my_data->recv_offset;
    if (send_size < (sizeof(int)*4)) {
      send_size = sizeof(int) * 4;
    }
  }
  else {
    width       = my_data->recv_width;
    send_size   = my_data->rsp_size;
    send_align  = my_data->send_align;
    send_offset = my_data->send_offset;
    recv_size   = my_data->req_size;
    recv_align  = my_data->recv_align;
    recv_offset = my_data->recv_offset;
    if (recv_size < (sizeof(int)*4)) {
      recv_size = sizeof(int) * 4;
    }
  }

  if (send_align < sizeof(int)) {
    send_align = sizeof(int);
  }
  else {
    send_align = (send_align + sizeof(int) - 1) & ~(sizeof(int) - 1);
  }
  
  if (recv_align < sizeof(int)) {
    recv_align = sizeof(int);
  }
  else {
    recv_align = (recv_align + sizeof(int) - 1) & ~(sizeof(int) - 1);
  }
  
  send_malloc_size = send_size + send_align + send_offset;
  recv_malloc_size = recv_size + recv_align + recv_offset;

  prev_link = NULL;
  for (i = 0; i < width; i++) {
    temp_link = (vst_ring_ptr)malloc(sizeof(vst_ring_elt_t));
        
    temp_link->send_buff_base = (char *)malloc(send_malloc_size);
    if (temp_link->send_buff_base == NULL) {
      report_test_failure(test,
                          (char *)__func__,
                          VSTE_MALLOC_FAILED,
                          "error allocating vst send buffer");
    }
    temp_link->send_buff_ptr  = temp_link->send_buff_base + send_offset;
    temp_link->send_buff_ptr  = (char *)(
                              ( (long)(temp_link->send_buff_ptr)
                              + (long)send_align - 1)
                              & ~((long)send_align - 1));
    temp_link->send_buff_size = send_malloc_size;
    temp_link->send_size      = send_size;

    temp_link->recv_buff_base = (char *)malloc(recv_malloc_size);
    if (temp_link->send_buff_base == NULL) {
      report_test_failure(test,
                          (char *)__func__,
                          VSTE_MALLOC_FAILED,
                          "error allocating vst recv buffer");
    }
    temp_link->recv_buff_ptr  = temp_link->recv_buff_base + recv_offset;
    temp_link->recv_buff_ptr  = (char *)(
                              ( (long)(temp_link->recv_buff_ptr)
                              + (long)recv_align - 1)
                              & ~((long)recv_align - 1));
    temp_link->recv_buff_size = recv_malloc_size;
    temp_link->recv_size      = recv_size;
   
    if (send) {
      memset(temp_link->send_buff_base, -1, send_malloc_size);
      send_buf    = (int *)temp_link->send_buff_ptr;
      send_buf[0] = send_size;
      send_buf[1] = recv_size;
    }

    temp_link->distribution   = NULL;
    
    if (i==0) {
      my_data->vst_ring = temp_link;
    }
    temp_link->next = prev_link;
    prev_link       = temp_link;
  }
  my_data->vst_ring->next = temp_link;
}


static void 
allocate_pattern_buffer(test_t *test)
{
  vst_data_t  *my_data;
  xmlNodePtr   wld;
  xmlNodePtr   pattern;
  xmlNodePtr   entry;
  vst_ring_ptr pattern_start[MAX_PATTERNS];
  dist_t      *dist[MAX_PATTERNS];
  vst_ring_ptr temp_link;
  vst_ring_ptr prev_link;
  char        *string;
  int          malloc_size;
  int          send_size;
  int          send_align;
  int          send_offset;
  int          recv_size;
  int          recv_align;
  int          recv_offset;
  int          seed;
  int          i;
  int          p;
  int          np;
  int          num;
  int         *send_buf;
  

  my_data = GET_TEST_DATA(test);
  wld     = my_data->wld;

  seed        = 0;
  send_align  = my_data->send_align;
  send_offset = my_data->send_offset;
  recv_align  = my_data->recv_align;
  recv_offset = my_data->recv_offset;

  if (send_align < sizeof(int)) {
    send_align = sizeof(int);
  }
  else {
    send_align = (send_align + sizeof(int) - 1) & ~(sizeof(int) - 1);
  }
  
  if (recv_align < sizeof(int)) {
    recv_align = sizeof(int);
  }
  else {
    recv_align = (recv_align + sizeof(int) - 1) & ~(sizeof(int) - 1);
  }
  
  pattern = wld->xmlChildrenNode;

  for (p=0;p < MAX_PATTERNS; p++) {
    pattern_start[p] = NULL;
    dist[p]          = NULL;
  }

  prev_link = NULL;
  while (pattern != NULL) {
    if (!xmlStrcmp(pattern->name,(const xmlChar *)"pattern")) {
      /* build entries for this pattern */
      string  =  (char *)xmlGetProp(pattern,(const xmlChar *)"index");
      p       =  atoi(string);
      i       =  0;
      entry = pattern->xmlChildrenNode;
      while (entry != NULL) {
        if (!xmlStrcmp(entry->name,(const xmlChar *)"pattern_entry")) {
          temp_link = (vst_ring_ptr)malloc(sizeof(vst_ring_elt_t));
          string    =  (char *)xmlGetProp(entry,(const xmlChar *)"req_size");
          send_size =  atoi(string);
          string    =  (char *)xmlGetProp(entry,(const xmlChar *)"rsp_size");
          recv_size =  atoi(string);

          if (send_size < (sizeof(int)*4)) {
            send_size = sizeof(int) * 4;
          }
          malloc_size = send_size + send_align + send_offset;
        
          temp_link->send_buff_base = (char *)malloc(malloc_size);
          if (temp_link->send_buff_base == NULL) {
            report_test_failure(test,
                                (char *)__func__,
                                VSTE_MALLOC_FAILED,
                                "error allocating vst send buffer");
          }
          temp_link->send_buff_ptr  = temp_link->send_buff_base + send_offset;
          temp_link->send_buff_ptr  = (char *)(
                                    ( (long)(temp_link->send_buff_ptr)
                                    + (long)send_align - 1)
                                    & ~((long)send_align - 1));
          temp_link->send_buff_size = malloc_size;
          temp_link->send_size      = send_size;

          memset(temp_link->send_buff_base, -1, malloc_size);

          malloc_size = recv_size + recv_align + recv_offset;

          temp_link->recv_buff_base = (char *)malloc(malloc_size);
          if (temp_link->send_buff_base == NULL) {
            report_test_failure(test,
                                (char *)__func__,
                                VSTE_MALLOC_FAILED,
                                "error allocating vst recv buffer");
          }
          temp_link->recv_buff_ptr  = temp_link->recv_buff_base + recv_offset;
          temp_link->recv_buff_ptr  = (char *)(
                                    ( (long)(temp_link->recv_buff_ptr)
                                    + (long)recv_align - 1)
                                    & ~((long)recv_align - 1));
          temp_link->recv_buff_size = malloc_size;
          temp_link->recv_size      = recv_size;
        
          send_buf    = (int *)temp_link->send_buff_ptr;
          
          send_buf[0] = send_size;
          send_buf[1] = recv_size;
          send_buf[2] = send_size;
          send_buf[3] = recv_size;

          temp_link->distribution   = NULL;

          if (test->debug) {
            fprintf(test->where,
                    "%s: pattern[%2d,%3d]  send_size =%6d  recv_size =%6d\n",
                    __func__, p, i, send_size, recv_size);
            fflush(test->where);
          }
          if (i==0) {
            pattern_start[p] = temp_link;
          }
          if (prev_link != NULL) {
            prev_link->next = temp_link;
          }
          prev_link = temp_link;
          i++;
        }
        entry = entry->next;
      }
    }
    if (!xmlStrcmp(pattern->name,(const xmlChar *)"distribution")) {
      /* build entry for the distribution after this pattern */
      dist[p] = (dist_t *)malloc(sizeof(dist_t));
      for (np=0; np<MAX_PATTERNS; np++) {
        dist[p]->pattern[np]    = NULL;
        dist[p]->dist_count[np] = -1;
      }
      string  = (char *)xmlGetProp(pattern,(const xmlChar *)"dist_key");
      dist[p]->dist_key = atoi(string);
      initstate(seed,dist[p]->random_state,VST_RAND_STATE_SIZE);
      entry = pattern->xmlChildrenNode;
      while (entry != NULL) {
        if (!xmlStrcmp(entry->name,(const xmlChar *)"distribution_entry")) {
          string  =  (char *)xmlGetProp(entry,(const xmlChar *)"next_pattern");
          np      =  atoi(string);
          string  =  (char *)xmlGetProp(entry,(const xmlChar *)"number");
          num     =  atoi(string);
          dist[p]->dist_count[np] = num;
          if (test->debug) {
            fprintf(test->where,
                    "%s: dist[%2d,%3d]  dist_number =%6d\n",
                    __func__, p, np, num);
            fflush(test->where);
          }
        }
        entry = entry->next;
      } 
      temp_link->distribution = dist[p];
    } 
    pattern = pattern->next;
  }
  for (np=0; np < p; np++) {
    dist[np]->num_patterns = p;
    for (i=0; i < p; i++) {
      dist[np]->pattern[i] = pattern_start[i];
    }
  }
  temp_link->next   = pattern_start[0];
  my_data->vst_ring = pattern_start[0];
}


static void
parse_wld_file(test_t *test, char *fname)
{
  xmlDocPtr   doc;
  xmlNodePtr  root;
  xmlNsPtr    ns;
  vst_data_t *my_data;
  int         send;

  my_data = GET_TEST_DATA(test);

  if (!xmlStrcmp(test->test_name,(const xmlChar *)"send_vst_rr")) {
    send = 1;
  }
  else {
    send = 0;
  }

  if (fname == NULL) {
    if (send) {
      fprintf(test->where,
              "WARNING %s: called %s without a work load description file\n",
              test->test_name, __func__);
      fprintf(test->where,
              "WARNING %s: test will be setup and run with fixed buffers\n",
              test->test_name);
      fflush(test->where);
    }
  }
  else if ((doc = xmlParseFile(fname)) != NULL) {
    if ((root = xmlDocGetRootElement(doc)) != NULL) {
      /* now make sure that the netperf namespace is present */
      ns = xmlSearchNsByHref(doc, root,
                  (const xmlChar *)"http://www.netperf.org/ns/netperf");
      if (ns != NULL) {
        if (!xmlStrcmp(root->name,(const xmlChar *)"work_load_description")) {
          /* happy day valid document */
          my_data->wld = root;
        }
        else {
          /* not a correct document type*/
          fprintf(test->where,
                  "file %s is of type \"%s\" not \"%s\"\n",
                  fname,root->name,"work_load_description");
          fflush(test->where);
          xmlFreeDoc(doc);
          doc = NULL;
        }
      }
      else {
        /* no namespace match */
        fprintf(test->where,
                "file %s does not reference a netperf namespace\n",fname);
        fflush(test->where);
        xmlFreeDoc(doc);
        doc = NULL;
      }
    }
    else {
      /* empty document */
      fprintf(test->where,
              "file %s contains no root element\n",fname);
      xmlFreeDoc(doc);
      fflush(test->where);
      doc = NULL;
    }
    if (doc == NULL) {
      fprintf(test->where,
              "WARNING %s: fill file '%s' work load description invalid\n",
              test->test_name, fname);
      fprintf(test->where,
              "WARNING %s: test will be setup and run with fixed buffers\n",
              test->test_name);
      fflush(test->where);
    }
  }
}


 /* This routine reads and parses the work_load_description and sets up
    the buffer ring with two sets of buffers one for send and one for 
    receive. */
static void
allocate_vst_buffers(test_t *test, char *fname)
{
  xmlNodePtr   wld;
  vst_data_t  *my_data;

  my_data = GET_TEST_DATA(test);

  parse_wld_file(test, fname);

  wld = my_data->wld;
  if (wld == NULL) {
    allocate_fixed_buffers(test);
  }
  else {
    allocate_pattern_buffer(test);
  }
}


static void
get_next_vst_transaction(test_t *test)
{
  vst_data_t  *my_data;
  dist_t      *dist;
  long         key;
  int          value;
  int          i;
  int          loc_debug = 0;

  my_data = GET_TEST_DATA(test);
  dist    = my_data->vst_ring->distribution;

  if (dist == NULL) {
    my_data->vst_ring = my_data->vst_ring->next;
  }
  else {
    /* we have reached the end of a pattern it is time to determine which
       pattern should be next in the sequence sgb 2005/11/21 */
    setstate(dist->random_state);
    key   = random();
    value = key %  dist->dist_key;
    if (test->debug && loc_debug) {
      fprintf(test->where, "**** end of pattern reached ******\n");
      fprintf(test->where,
              "%s:  value = %d  key = %ld\n",
              (char *)__func__, value, key);
      fflush(test->where);
    }
    for (i=0; i< dist->num_patterns; i++) {
      value = value - dist->dist_count[i];
      if (test->debug && loc_debug) {
        fprintf(test->where,
                "\tdist_count = %d  new_value = %d  pattern = %ld\n",
                dist->dist_count[i], value, dist->pattern[i]);
        fflush(test->where);
      }
      if (value < 0) {
        my_data->vst_ring = dist->pattern[i];
        break;
      }
    }
    if (test->debug && loc_debug) {
      fprintf(test->where,
              "%s: new ring value %p\n",
              __func__, my_data->vst_ring);
      fflush(test->where);
    }
  }
}


 /* This routine will create a data (listen) socket with the apropriate */
 /* options set and return it to the caller. this replaces all the */
 /* duplicate code in each of the test routines and should help make */
 /* things a little easier to understand. since this routine can be */
 /* called by either the netperf or netserver programs, all output */
 /* should be directed towards "where." family is generally AF_INET, */
 /* and type will be either SOCK_STREAM or SOCK_DGRAM */
static int
create_data_socket(test_t *test)
{
  vst_data_t  *my_data = GET_TEST_DATA(test);

  int family           = my_data->locaddr->ai_family;
  int type             = my_data->locaddr->ai_socktype;
  int lss_size         = my_data->send_buf_size;
  int lsr_size         = my_data->recv_buf_size;
  int loc_sndavoid     = my_data->send_avoid;
  int loc_rcvavoid     = my_data->recv_avoid;

  int temp_socket;
  int one;
  netperf_socklen_t sock_opt_len;

  if (test->debug) {
    fprintf(test->where,
            "%s: calling socket family = %d type = %d\n",
            __func__, family, type);
    fflush(test->where);
  }
  /*set up the data socket                        */
  temp_socket = socket(family,
                       type,
                       0);

  if (CHECK_FOR_INVALID_SOCKET) {
    report_test_failure(test,
                        (char *)__func__,
                        VSTE_SOCKET_ERROR,
                        "error creating socket");
    return(temp_socket);
  }

  if (test->debug) {
    fprintf(test->where,
            "%s: socket %d obtained...\n",
            __func__, temp_socket);
    fflush(test->where);
  }

  /* Modify the local socket size. The reason we alter the send buffer */
  /* size here rather than when the connection is made is to take care */
  /* of decreases in buffer size. Decreasing the window size after */
  /* connection establishment is a TCP no-no. Also, by setting the */
  /* buffer (window) size before the connection is established, we can */
  /* control the TCP MSS (segment size). The MSS is never more that 1/2 */
  /* the minimum receive buffer size at each half of the connection. */
  /* This is why we are altering the receive buffer size on the sending */
  /* size of a unidirectional transfer. If the user has not requested */
  /* that the socket buffers be altered, we will try to find-out what */
  /* their values are. If we cannot touch the socket buffer in any way, */
  /* we will set the values to -1 to indicate that.  */

#ifdef SO_SNDBUF
  if (lss_size > 0) {
    if(setsockopt(temp_socket, SOL_SOCKET, SO_SNDBUF,
                  &lss_size, sizeof(int)) < 0) {
      report_test_failure(test,
                          (char *)__func__,
                          VSTE_SETSOCKOPT_ERROR,
                          "error setting local send socket buffer size");
      return(temp_socket);
    }
    if (test->debug > 1) {
      fprintf(test->where,
              "%s: %s: SO_SNDBUF of %d requested.\n",
              __FILE__, __func__, lss_size);
      fflush(test->where);
    }
  }

  if (lsr_size > 0) {
    if(setsockopt(temp_socket, SOL_SOCKET, SO_RCVBUF,
                  &lsr_size, sizeof(int)) < 0) {
      report_test_failure(test,
                          (char *)__func__,
                          VSTE_SETSOCKOPT_ERROR,
                          "error setting local recv socket buffer size");
      return(temp_socket);
    }
    if (test->debug > 1) {
      fprintf(test->where,
              "%s: %s: SO_RCVBUF of %d requested.\n",
              __FILE__, __func__, lsr_size);
      fflush(test->where);
    }
  }

  /* Now, we will find-out what the size actually became, and report */
  /* that back to the test. If the call fails, we will just report a -1 */
  /* back to the initiator for the recv buffer size. */

  sock_opt_len = sizeof(int);
  if (getsockopt(temp_socket,
                 SOL_SOCKET,
                 SO_SNDBUF,
                 (char *)&lss_size,
                 &sock_opt_len) < 0) {
    fprintf(test->where,
        "%s: %s: getsockopt SO_SNDBUF: errno %d\n",
        __FILE__, __func__, errno);
    fflush(test->where);
    lss_size = -1;
  }
  if (getsockopt(temp_socket,
                 SOL_SOCKET,
                 SO_RCVBUF,
                 (char *)&lsr_size,
                 &sock_opt_len) < 0) {
    fprintf(test->where,
        "%s: %s: getsockopt SO_RCVBUF: errno %d\n",
        __FILE__, __func__, errno);
    fflush(test->where);
    lsr_size = -1;
  }
  if (test->debug) {
    fprintf(test->where,
            "%s: %s: socket sizes determined...\n",
            __FILE__, __func__);
    fprintf(test->where,
            "                       send: %d recv: %d\n",
            lss_size,lsr_size);
    fflush(test->where);
  }

#else /* SO_SNDBUF */

  lss_size = -1;
  lsr_size = -1;

#endif /* SO_SNDBUF */

  my_data->sbuf_size_ret = lss_size;
  my_data->rbuf_size_ret = lsr_size;
  /* now, we may wish to enable the copy avoidance features on the */
  /* local system. of course, this may not be possible... */

#ifdef SO_RCV_COPYAVOID
  if (loc_rcvavoid) {
    if (setsockopt(temp_socket,
                   SOL_SOCKET,
                   SO_RCV_COPYAVOID,
                   &loc_rcvavoid,
                   sizeof(int)) < 0) {
      fprintf(test->where,
              "%s: %s: Could not enable receive copy avoidance",
              __FILE__, __func__);
      fflush(test->where);
      loc_rcvavoid = 0;
    }
  }
#else
  /* it wasn't compiled in... */
  loc_rcvavoid = 0;
#endif /* SO_RCV_COPYAVOID */

#ifdef SO_SND_COPYAVOID
  if (loc_sndavoid) {
    if (setsockopt(temp_socket,
                   SOL_SOCKET,
                   SO_SND_COPYAVOID,
                   &loc_sndavoid,
                   sizeof(int)) < 0) {
      fprintf(test->where,
        "%s: %s: Could not enable send copy avoidance\n",
        __FILE__, __func__);
      fflush(test->where);
      loc_sndavoid = 0;
    }
  }
#else
  /* it was not compiled in... */
  loc_sndavoid = 0;
#endif

  /* Now, we will see about setting the TCP_NO_DELAY flag on the local */
  /* socket. We will only do this for those systems that actually */
  /* support the option. If it fails, note the fact, but keep going. */
  /* If the user tries to enable TCP_NODELAY on a UDP socket, this */
  /* will cause an error to be displayed */

#ifdef TCP_NODELAY
  if (my_data->no_delay) {
    one = 1;
    if(setsockopt(temp_socket,
                  getprotobyname("tcp")->p_proto,
                  TCP_NODELAY,
                  (char *)&one,
                  sizeof(one)) < 0) {
      fprintf(test->where,
              "%s: %s: nodelay: errno %d\n",
              __FILE__, __func__, errno);
      fflush(test->where);
    }

    if (test->debug > 1) {
      fprintf(test->where,
              "%s: %s: TCP_NODELAY requested...\n",
              __FILE__, __func__);
      fflush(test->where);
    }
  }
#else /* TCP_NODELAY */

  my_data->no_delay = 0;

#endif /* TCP_NODELAY */

  return(temp_socket);

}


/* free all the data structures allocated by the tests other than
   the vst_data_t allocated by vst_test_init that will be done by
   wait_to_die.    sgb  2005/11/21 */

static void
free_vst_test_data(test_t *test)
{
  vst_data_t   *my_data;
  vst_ring_ptr  prev;
  vst_ring_ptr  curr;
  
  my_data     = GET_TEST_DATA(test);

  xmlFreeNode(my_data->wld);
  prev        = my_data->vst_ring;
  curr        = prev->next;
  prev->next  = NULL;
  while (curr) {
    /* free memory allocate for vst ring */
    free(curr->send_buff_base);
    free(curr->recv_buff_base);
    free(curr->distribution);
    prev = curr;
    curr = curr->next;
    free(prev);
  }
  free(my_data->locaddr);
  free(my_data->remaddr);
  fclose(my_data->fill_source);
  my_data->vst_ring    = NULL;
  my_data->locaddr     = NULL;
  my_data->remaddr     = NULL;
  my_data->fill_source = NULL;
}


/*  Initialize the data structures for the variable sized data tests.
    The tests default to the same behavior as a tcp_rr test unless
    the file_file is specified and contains a valid xml document
    which specifies a valid work_load_description.
    2005-11-18  sgb */

static vst_data_t *
vst_test_init(test_t *test, int type, int protocol)
{
  vst_data_t *new_data;
  xmlNodePtr  args;
  xmlChar    *string;
  char       *fname;
  xmlChar    *units;
  xmlChar    *localhost;
  xmlChar    *localport;
  int         localfam;

  int               count;
  int               error;
  struct addrinfo   hints;
  struct addrinfo  *local_ai;

  NETPERF_DEBUG_ENTRY(test->debug, test->where);

  /* allocate memory to store the information about this test */
  new_data = (vst_data_t *)malloc(sizeof(vst_data_t));

  args = test->node->xmlChildrenNode;
  while (args != NULL) {
    if (!xmlStrcmp(args->name,(const xmlChar *)"dependency_data")) {
      test->dependency_data = args;
    }
    if (xmlStrcmp(args->name,(const xmlChar *)"socket_args")) {
      args = args->next;
      continue;
    } 
    break;
  }
 
  /* probably a good idea to make sure that new_data is real */
  if ((args != NULL) &&
      (NULL != new_data)) {
    /* zero the vst test specific data structure */
    memset(new_data,0,sizeof(vst_data_t));

    fname  =  (char *)xmlGetProp(args,(const xmlChar *)"fill_file");
    /* fopen the fill file it will be used when allocating buffer rings */
    if (fname) {
      new_data->fill_source = fopen(fname,"r");
    }

    /* we are relying on the good graces of the validating and
       attribute filling of libxml when we parsed the message that got
       us here... */
    string =  xmlGetProp(args,(const xmlChar *)"send_buffer_size");
    units  =  xmlGetProp(args,(const xmlChar *)"send_buffer_units");
    new_data->send_buf_size = convert(string,units);

    string =  xmlGetProp(args,(const xmlChar *)"send_size");
    units  =  xmlGetProp(args,(const xmlChar *)"send_size_units");
    new_data->send_size = convert(string,units);

    string =  xmlGetProp(args,(const xmlChar *)"recv_buffer_size");
    units  =  xmlGetProp(args,(const xmlChar *)"recv_buffer_units");
    new_data->recv_buf_size = convert(string,units);

    string =  xmlGetProp(args,(const xmlChar *)"recv_size");
    units  =  xmlGetProp(args,(const xmlChar *)"recv_size_units");
    new_data->recv_size = convert(string,units);

    string =  xmlGetProp(args,(const xmlChar *)"req_size");
    new_data->req_size = atoi((char *)string);

    string =  xmlGetProp(args,(const xmlChar *)"rsp_size");
    new_data->rsp_size = atoi((char *)string);

    /* relying on the DTD to give us defaults isn't always the most
       robust way to go about doing things. */
    string =  xmlGetProp(args,(const xmlChar *)"port_min");
    if (string) {
      new_data->port_min = atoi((char *)string);
    }
    else {
      new_data->port_min = -1;
    }

    string =  xmlGetProp(args,(const xmlChar *)"port_max");
    if (string) {
      new_data->port_max = atoi((char *)string);
    }
    else {
      new_data->port_min = -1;
    }
    
    string =  xmlGetProp(args,(const xmlChar *)"send_width");
    new_data->send_width = atoi((char *)string);
    if (new_data->send_width == 0) {
      new_data->send_width = new_data->send_buf_size/new_data->send_size + 1;
      if (new_data->send_width == 1) new_data->send_width = 2;
    }

    string =  xmlGetProp(args,(const xmlChar *)"recv_width");
    new_data->recv_width = atoi((char *)string);
    if (new_data->recv_width == 0) {
      new_data->recv_width = new_data->recv_buf_size/new_data->recv_size + 1;
      if (new_data->recv_width == 1) new_data->recv_width = 2;
    }

    string =  xmlGetProp(args,(const xmlChar *)"send_align");
    new_data->send_align = atoi((char *)string);

    string =  xmlGetProp(args,(const xmlChar *)"recv_align");
    new_data->recv_align = atoi((char *)string);

    string =  xmlGetProp(args,(const xmlChar *)"send_offset");
    new_data->send_offset = atoi((char *)string);

    string =  xmlGetProp(args,(const xmlChar *)"recv_offset");
    new_data->recv_offset = atoi((char *)string);

    /* now get and initialize the local addressing info */
    string   =  xmlGetProp(args,(const xmlChar *)"family");
    localfam = strtofam(string);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = localfam;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    hints.ai_flags    = 0;

    localhost = xmlGetProp(args,(const xmlChar *)"local_host"),
    localport = xmlGetProp(args,(const xmlChar *)"local_service"),
    count = 0;
    do {
      error = getaddrinfo( (char *)localhost, (char *)localport,
                              &hints, &local_ai);
      count += 1;
      if (error == EAI_AGAIN) {
        if (test->debug) {
          fprintf(test->where,"Sleeping on getaddrinfo EAI_AGAIN\n");
          fflush(test->where);
        }
        sleep(1);
      }
    } while ((error == EAI_AGAIN) && (count <= 5));
    
    if (test->debug) {
      dump_addrinfo(test->where, local_ai, localhost, localport, localfam);
    }

    if (!error) {
      new_data->locaddr = local_ai;
    }
    else {
      if (test->debug) {
        fprintf(test->where,"%s: getaddrinfo returned %d %s\n",
                __func__, error, gai_strerror(error));
        fflush(test->where);
      }
      report_test_failure(test,
                          (char *)__func__,
                          VSTE_GETADDRINFO_ERROR,
                          gai_strerror(error));
    }
  }
  else {
    report_test_failure(test,
                        (char *)__func__,
                        VSTE_NO_SOCKET_ARGS,
                        "no socket_arg element was found");
  }

  SET_TEST_DATA(test, new_data);

  allocate_vst_buffers(test, fname);

  NETPERF_DEBUG_EXIT(test->debug, test->where);

  return(new_data);
}

static void
update_elapsed_time(vst_data_t *my_data)
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
vst_test_clear_stats(vst_data_t *my_data)
{
  int i;
  for (i = 0; i < VST_MAX_COUNTERS; i++) {
    my_data->stats.counter[i] = 0;
  }
  my_data->elapsed_time.tv_usec = 0;
  my_data->elapsed_time.tv_sec  = 0;
  gettimeofday(&(my_data->prev_time),NULL);
  my_data->curr_time = my_data->prev_time;
  return(NPE_SUCCESS);
}

static void
vst_test_decode_stats(xmlNodePtr stats,test_t *test)
{
  if (test->debug) {
    fprintf(test->where,"%s: entered for %s test %s\n",
            __func__, test->id, test->test_name);
    fflush(test->where);
  }
}

static xmlNodePtr
vst_test_get_stats(test_t *test)
{
  xmlNodePtr  stats = NULL;
  xmlAttrPtr  ap    = NULL;
  int         i;
  char        value[32];
  char        name[32];
  uint64_t    loc_cnt[VST_MAX_COUNTERS];

  vst_data_t *my_data = GET_TEST_DATA(test);

  if (test->debug) {
    fprintf(test->where,"%s: entered for %s test %s\n",
            __func__, test->id, test->test_name);
    fflush(test->where);
  }
  if ((stats = xmlNewNode(NULL,(xmlChar *)"test_stats")) != NULL) {
    /* set the properites of the test_stats message -
       the tid and time stamps/values and counter values  sgb 2004-07-07 */

    ap = xmlSetProp(stats,(xmlChar *)"tid",test->id);
    for (i = 0; i < VST_MAX_COUNTERS; i++) {
      loc_cnt[i] = my_data->stats.counter[i];
      if (test->debug) {
        fprintf(test->where,"VST_COUNTER%X = %#"PRIx64"\n",i,loc_cnt[i]);
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
    for (i = 0; i < VST_MAX_COUNTERS; i++) {
      if (ap == NULL) {
        break;
      }
      if (loc_cnt[i]) {
        sprintf(value,"%#"PRIx64,my_data->stats.counter[i]);
        sprintf(name,"cntr%1X_value",i);
        ap = xmlSetProp(stats,(xmlChar *)name,(xmlChar *)value);
        if (test->debug) {
          fprintf(test->where,"%s=%s\n",name,value);
          fflush(test->where);
        }
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
} /* end of vst_test_get_stats */


static void
recv_vst_rr_preinit(test_t *test)
{
  int               rc;
  int               s_listen;
  vst_data_t       *my_data;
  struct sockaddr   myaddr;
  netperf_socklen_t mylen;

  my_data   = GET_TEST_DATA(test);
  mylen     = sizeof(myaddr);

  s_listen = create_data_socket(test);
  my_data->s_listen = s_listen;
  if (test->debug) {
    dump_addrinfo(test->where, my_data->locaddr,
                  (xmlChar *)NULL, (xmlChar *)NULL, -1);
    fprintf(test->where, 
            "%s:create_data_socket returned %d\n", 
            __func__, s_listen);
    fflush(test->where);
  }
  rc = bind(s_listen, my_data->locaddr->ai_addr, my_data->locaddr->ai_addrlen);
  if (test->debug) {
    fprintf(test->where, 
            "%s:bind returned %d  errno=%d\n", 
            __func__, rc, errno);
    fflush(test->where);
  }
  if (rc == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        VSTE_BIND_FAILED,
                        "data socket bind failed");
  } else if (listen(s_listen,5) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        VSTE_LISTEN_FAILED,
                        "data socket listen failed");
  } else if (getsockname(s_listen,&myaddr,&mylen) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        VSTE_GETSOCKNAME_FAILED,
                        "getting the listen socket name failed");
  }
  else {
    memcpy(my_data->locaddr->ai_addr,&myaddr,mylen);
    my_data->locaddr->ai_addrlen = mylen;
    set_dependent_data(test);
  }
}

static uint32_t
recv_vst_rr_init(test_t *test)
{
  int               s_data;
  vst_data_t       *my_data;
  struct sockaddr   peeraddr;
  netperf_socklen_t peerlen;

  my_data   = GET_TEST_DATA(test);
  peerlen   = sizeof(peeraddr);

  if (test->debug) {
    fprintf(test->where, "%s:waiting in accept\n", __func__);
    fflush(test->where);
  }
  if ((s_data = accept(my_data->s_listen,
                      &peeraddr,
                      &peerlen)) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        VSTE_ACCEPT_FAILED,
                        "listen socket accept failed");
  }
  else {
    if (test->debug) {
      fprintf(test->where, 
              "%s:accept returned successfully %d\n", 
              __func__, s_data);
      fflush(test->where);
    }
    my_data->s_data = s_data;
  }
  return(TEST_IDLE);
}

static void
recv_vst_rr_idle_link(test_t *test, int last_len)
{
  int               len;
  uint32_t          new_state;
  vst_data_t       *my_data;
  struct sockaddr   peeraddr;
  netperf_socklen_t peerlen;

  NETPERF_DEBUG_ENTRY(test->debug, test->where);

  my_data   = GET_TEST_DATA(test);
  len       = last_len;
  peerlen   = sizeof(peeraddr);

  new_state = CHECK_REQ_STATE;
  while (new_state == TEST_LOADED) {
    sleep(1);
    new_state = CHECK_REQ_STATE;
  }

  if (new_state == TEST_IDLE) {
    if (test->debug) {
      fprintf(test->where,"%s: transition from LOAD to IDLE\n", __func__);
      fflush(test->where);
    }
    if (shutdown(my_data->s_data,SHUT_WR) == -1) {
      report_test_failure(test,
                          (char *)__func__,
                          VSTE_SOCKET_SHUTDOWN_FAILED,
                          "data_recv_error");
    }
    else {
      while (len > 0) {
        len=recv(my_data->s_data,
                 my_data->vst_ring->recv_buff_ptr,
                 my_data->vst_ring->recv_size, 0);
      }
      close(my_data->s_data);
      if (test->debug) {
        fprintf(test->where,"%s: waiting in accept\n", __func__);
        fflush(test->where);
      }
      if ((my_data->s_data=accept(my_data->s_listen,
                                 &peeraddr,
                                 &peerlen)) == -1) {
        report_test_failure(test,
                          (char *)__func__,
                          VSTE_ACCEPT_FAILED,
                          "listen socket accept failed");
      }
      else {
        if (test->debug) {
          fprintf(test->where,
                  "%s: accept returned successfully %d\n",
                  __func__,
                  my_data->s_data);
          fflush(test->where);
        }
      }
    }
  }
  else {
    /* a transition to a state other than TEST_IDLE was requested
       after the link was closed in the TEST_LOADED state */
    report_test_failure(test,
                        (char *)__func__,
                        VSTE_DATA_CONNECTION_CLOSED_ERROR,
                        "data connection closed and non idle state requested");
  }
}

static uint32_t
recv_vst_rr_meas(test_t *test)
{
  int               len;
  int               bytes_left;
  int               received;
  int              *req_base;
  int               rsp_size;
  char             *req_ptr;
  uint32_t          new_state;
  vst_data_t       *my_data;

  HISTOGRAM_VARS;
  my_data   = GET_TEST_DATA(test);

  while (NO_STATE_CHANGE(test)) {
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_one);
    /* recv the request for the test */
    req_base   = (int *)my_data->vst_ring->recv_buff_ptr;
    req_ptr    = my_data->vst_ring->recv_buff_ptr;
    bytes_left = my_data->vst_ring->recv_size;
    received   = 0;
    while (bytes_left > 0) {
      if ((len=recv(my_data->s_data,
                    req_ptr,
                    bytes_left,
                    0)) != 0) {
        /* this macro hides windows differences */
        if (CHECK_FOR_RECV_ERROR(len)) {
          report_test_failure(test,
                              (char *)__func__,
                              VSTE_DATA_RECV_ERROR,
                              "data_recv_error");
          break;
        }
        my_data->stats.named.bytes_received += len;
        my_data->stats.named.recv_calls++;
        received  += len;
        req_ptr   += len;
        if (received >= (sizeof(int)*4)) {
          bytes_left = ntohl(req_base[0]);
          rsp_size   = ntohl(req_base[1]);
          if ((bytes_left > my_data->vst_ring->recv_size) ||
              (rsp_size   > my_data->vst_ring->send_size) ||
              (bytes_left < (sizeof(int)*4)) || 
              (rsp_size   < (sizeof(int)*4)) ||
              (bytes_left != ntohl(req_base[2])) ||
              (rsp_size   != ntohl(req_base[3])) ) {
            fprintf(test->where,
                    "\n%s: Error in received packet for test_id = '%s'\n",
                    __func__, test->id);
            fprintf(test->where,
                    "\treq_base[0] = %d\treq_base[1] = %d\n",
                    bytes_left, rsp_size);
            fprintf(test->where,
                    "\treq_base[2] = %d\treq_base[3] = %d\n\n",
                    ntohl(req_base[2]), ntohl(req_base[3]));
            fprintf(test->where,
                    "\treq_base = %p\torig_req_ptr = %p\treceived = %d\n",
                    req_base, my_data->vst_ring->recv_buff_ptr, received);
            fflush(test->where);
          }
          bytes_left = bytes_left - received;
        }
        else {
          bytes_left = my_data->vst_ring->recv_size - received;
        }
      }
      else {
        /* just got a data connection close break out of while loop */
        break;
      }
    }
    if (len == 0) {
      /* how do we deal with a closed connection in the measured state */
      report_test_failure(test,
                          (char *)__func__,
                          VSTE_DATA_CONNECTION_CLOSED_ERROR,
                          "data connection closed during TEST_MEASURE state");
    }
    else {
      my_data->stats.named.trans_received++;
      if ((len=send(my_data->s_data,
                    my_data->vst_ring->send_buff_ptr,
                    rsp_size,
                    0)) != rsp_size) {
        /* this macro hides windows differences */
        if (CHECK_FOR_SEND_ERROR(len)) {
          report_test_failure(test,
                              (char *)__func__,
                              VSTE_DATA_SEND_ERROR,
                              "data_send_error");
        }
      }
      my_data->stats.named.bytes_sent += len;
      my_data->stats.named.send_calls++;
    }
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_two);
    HIST_ADD(my_data->time_hist,&time_one,&time_two);
    get_next_vst_transaction(test);
  }
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_LOADED) {
    /* transitioning to loaded state from measure state
       set current timestamp and update elapsed time */
    gettimeofday(&(my_data->curr_time), NULL);
    update_elapsed_time(my_data);
  }
  return(new_state);
}

static uint32_t
recv_vst_rr_load(test_t *test)
{
  int               len;
  int               bytes_left;
  int               received;
  int              *req_base;
  int               rsp_size;
  char             *req_ptr;
  uint32_t          new_state;
  vst_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  while (NO_STATE_CHANGE(test)) {
    /* recv the request for the test */
    req_base   = (int *)my_data->vst_ring->recv_buff_ptr;
    req_ptr    = my_data->vst_ring->recv_buff_ptr;
    bytes_left = my_data->vst_ring->recv_size;
    received   = 0;
    while (bytes_left > 0) {
      if ((len=recv(my_data->s_data,
                    req_ptr,
                    bytes_left,
                    0)) != 0) {
        /* this macro hides windows differences */
        if (CHECK_FOR_RECV_ERROR(len)) {
          report_test_failure(test,
                              (char *)__func__,
                              VSTE_DATA_RECV_ERROR,
                              "data_recv_error");
          break;
        }
        received   += len;
        req_ptr    += len;
        if (received >= (sizeof(int)*4)) {
          bytes_left = ntohl(req_base[0]);
          rsp_size   = ntohl(req_base[1]);
          if ((bytes_left > my_data->vst_ring->recv_size) ||
              (rsp_size   > my_data->vst_ring->send_size) ||
              (bytes_left < (sizeof(int)*4)) || 
              (rsp_size   < (sizeof(int)*4)) ||
              (bytes_left != ntohl(req_base[2])) ||
              (rsp_size   != ntohl(req_base[3])) ) {
            fprintf(test->where,
                    "\n%s: Error in received packet for test_id = '%s'\n",
                    __func__, test->id);
            fprintf(test->where,
                    "\treq_base[0] = %d\treq_base[1] = %d\n",
                    bytes_left, rsp_size);
            fprintf(test->where,
                    "\treq_base[2] = %d\treq_base[3] = %d\n\n",
                    ntohl(req_base[2]), ntohl(req_base[3]));
            fprintf(test->where,
                    "\treq_base = %p\torig_req_ptr = %p\treceived = %d\n",
                    req_base, my_data->vst_ring->recv_buff_ptr, received);
            fflush(test->where);
          }
          bytes_left = bytes_left - received;
        }
        else {
          bytes_left = my_data->vst_ring->recv_size - received;
        }
      }
      else {
        /* just got a data connection close break out of while loop */
        break;
      }
    }
    /* check for state transition */
    new_state = CHECK_REQ_STATE;
    if ((len == 0) ||
        (new_state == TEST_IDLE)) {
      /* just got a data connection close or
         a request to transition to the idle state */
      break;
    } 
    if ((len=send(my_data->s_data,
                  my_data->vst_ring->send_buff_ptr,
                  rsp_size,
                  0)) != rsp_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            VSTE_DATA_SEND_ERROR,
                            "data_send_error");
      }
    }
    get_next_vst_transaction(test);
  }
  new_state = CHECK_REQ_STATE;
  if ((len == 0) ||
      (new_state == TEST_IDLE)) {
    recv_vst_rr_idle_link(test,len);
    new_state = TEST_IDLE;
  }
  else {
    if (new_state == TEST_MEASURE) {
      /* transitioning to measure state from loaded state
         set previous timestamp */
      gettimeofday(&(my_data->prev_time), NULL);
    }
  }
  return(new_state);
}

static void
send_vst_rr_preinit(test_t *test)
{
  vst_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  get_dependency_data(test, SOCK_STREAM, IPPROTO_TCP);
  my_data->s_data = create_data_socket(test);
}

static uint32_t
send_vst_rr_init(test_t *test)
{
  vst_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  if (test->debug) {
    fprintf(test->where,"%s: in INIT state making connect call\n", __func__);
    fflush(test->where);
  }
  if (connect(my_data->s_data,
              my_data->remaddr->ai_addr,
              my_data->remaddr->ai_addrlen) < 0) {
    report_test_failure(test,
                        (char *)__func__,
                        VSTE_CONNECT_FAILED,
                        "data socket connect failed");
  }
  else {
    if (test->debug) {
      fprintf(test->where,"%s: connected and moving to IDLE\n", __func__);
      fflush(test->where);
    }
  }
  return(TEST_IDLE);
}

static void
send_vst_rr_idle_link(test_t *test, int last_len)
{
  int               len;
  uint32_t          new_state;
  vst_data_t       *my_data;

  NETPERF_DEBUG_ENTRY(test->debug, test->where);

  my_data   = GET_TEST_DATA(test);
  len       = last_len;

  new_state = CHECK_REQ_STATE;
  while (new_state == TEST_LOADED) {
    sleep(1);
    new_state = CHECK_REQ_STATE;
  }
  if (new_state == TEST_IDLE) {
    if (test->debug) {
      fprintf(test->where,"%s: transition from LOAD to IDLE\n", __func__);
      fflush(test->where);
    }
    if (shutdown(my_data->s_data,SHUT_WR) == -1) {
      report_test_failure(test,
                          (char *)__func__,
                          VSTE_SOCKET_SHUTDOWN_FAILED,
                          "failure shuting down data socket");
    }
    else {
      while (len > 0) {
        len = recv(my_data->s_data,
                   my_data->vst_ring->recv_buff_ptr,
                   my_data->vst_ring->recv_size, 0);
      }
      close(my_data->s_data);
      my_data->s_data = create_data_socket(test);
      if (test->debug) {
        fprintf(test->where,"%s: connecting from LOAD state\n", __func__);
        fflush(test->where);
      }
      if (connect(my_data->s_data,
                  my_data->remaddr->ai_addr,
                  my_data->remaddr->ai_addrlen) < 0) {
        report_test_failure(test,
                            (char *)__func__,
                            VSTE_CONNECT_FAILED,
                            "data socket connect failed");
      }
      else {
        if (test->debug) {
          fprintf(test->where,"%s: connected moving to IDLE\n", __func__);
          fflush(test->where);
        }
      }
    }
  }
  else {
    /* a transition to a state other than TEST_IDLE was requested
       after the link was closed in the TEST_LOADED state */
    report_test_failure(test,
                        (char *)__func__,
                        VSTE_DATA_CONNECTION_CLOSED_ERROR,
                        "data connection closed and non idle state requested");

  }
}

static uint32_t
send_vst_rr_meas(test_t *test)
{
  uint32_t          new_state;
  int               len;
  int               send_len;
  int               bytes_left;
  char             *rsp_ptr;
  vst_data_t       *my_data;
  int              *send_buf;

  my_data   = GET_TEST_DATA(test);

  while (NO_STATE_CHANGE(test)) {
    HISTOGRAM_VARS;
    /* code to make data dirty macro enabled by DIRTY */
    MAKE_DIRTY(my_data,my_data->vst_ring);
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_one);
    /* send data for the test */
    send_len = my_data->vst_ring->send_size;
    bytes_left = my_data->vst_ring->recv_size;
    send_buf   = (int *)my_data->vst_ring->send_buff_ptr;
    if ((send_len   != send_buf[0]) ||
        (bytes_left != send_buf[1]) ||
        (send_len   != send_buf[2]) ||
        (bytes_left != send_buf[3])) {
      fprintf(test->where,
              "\n%s: Found corrupted buffer is %d,%d should be %d,%d\n\n",
              __func__, send_buf[0], send_buf[1], send_len, bytes_left);
      fflush(test->where);
    }
    if((len=send(my_data->s_data,
                 my_data->vst_ring->send_buff_ptr,
                 send_len,
                 0)) != send_len) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            VSTE_DATA_SEND_ERROR,
                            "data send error");
      }
    }
    my_data->stats.named.bytes_sent += len;
    my_data->stats.named.send_calls++;
    /* recv the request for the test */
    rsp_ptr    = my_data->vst_ring->recv_buff_ptr;
    while (bytes_left > 0) {
      if ((len=recv(my_data->s_data,
                    rsp_ptr,
                    bytes_left,
                    0)) != 0) {
        /* this macro hides windows differences */
        if (CHECK_FOR_RECV_ERROR(len)) {
          report_test_failure(test,
                              (char *)__func__,
                              VSTE_DATA_RECV_ERROR,
                              "data_recv_error");
          break;
        }
        my_data->stats.named.bytes_received += len;
        my_data->stats.named.recv_calls++;
        rsp_ptr    += len;
        bytes_left -= len;
      }
      else {
        /* len is 0 the connection was closed exit the while loop */
        break;
      }
    }
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_two);
    HIST_ADD(my_data->time_hist,&time_one,&time_two);
    my_data->stats.named.trans_sent++;
    get_next_vst_transaction(test);
    if (len == 0) {
      /* how do we deal with a closed connection in the measured state */
      report_test_failure(test,
                          (char *)__func__,
                          VSTE_DATA_CONNECTION_CLOSED_ERROR,
                          "data connection closed during TEST_MEASURE state");
    }
  }
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_LOADED) {
    /* transitioning to loaded state from measure state
       set current timestamp and update elapsed time */
    gettimeofday(&(my_data->curr_time), NULL);
    update_elapsed_time(my_data);
  }
  return(new_state);
}

static uint32_t
send_vst_rr_load(test_t *test)
{
  uint32_t          new_state;
  int               len;
  int               send_len;
  int               bytes_left;
  char             *rsp_ptr;
  vst_data_t       *my_data;
  int              *send_buf;

  my_data   = GET_TEST_DATA(test);

  while (NO_STATE_CHANGE(test)) {
    /* code to make data dirty macro enabled by DIRTY */
    MAKE_DIRTY(my_data,my_data->vst_ring);
    /* send data for the test */
    send_len   = my_data->vst_ring->send_size;
    bytes_left = my_data->vst_ring->recv_size;
    send_buf   = (int *)my_data->vst_ring->send_buff_ptr;
    if ((send_len   != send_buf[0]) ||
        (bytes_left != send_buf[1]) ||
        (send_len   != send_buf[2]) ||
        (bytes_left != send_buf[3])) {
      fprintf(test->where,
              "\n%s: Found corrupted buffer is %d,%d should be %d,%d\n\n",
              __func__, send_buf[0], send_buf[1], send_len, bytes_left);
      fflush(test->where);
    }
    if((len=send(my_data->s_data,
                 my_data->vst_ring->send_buff_ptr,
                 send_len,
                 0)) != send_len) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            VSTE_DATA_SEND_ERROR,
                            "data send error");
      }
    }
    /* recv the request for the test */
    rsp_ptr    = my_data->vst_ring->recv_buff_ptr;
    while (bytes_left > 0) {
      if ((len=recv(my_data->s_data,
                    rsp_ptr,
                    bytes_left,
                    0)) != 0) {
        /* this macro hides windows differences */
        if (CHECK_FOR_RECV_ERROR(len)) {
          report_test_failure(test,
                              (char *)__func__,
                              VSTE_DATA_RECV_ERROR,
                              "data_recv_error");
          break;
        }
        rsp_ptr    += len;
        bytes_left -= len;
      }
      else {
        /* len is 0 the connection was closed exit the while loop */
        break;
      }
    }
    if (len == 0) {
      break;
    }
    get_next_vst_transaction(test);
  }
  new_state = CHECK_REQ_STATE;
  if ((len == 0) ||
      (new_state == TEST_IDLE)) {
    send_vst_rr_idle_link(test,len);
    new_state = TEST_IDLE;
  }
  else {
    if (new_state == TEST_MEASURE) {
      /* transitioning to measure state from loaded state
         set previous timestamp */
      gettimeofday(&(my_data->prev_time), NULL);
    }
  }
  return(new_state);
}

int
recv_vst_rr_clear_stats(test_t *test)
{
  return(vst_test_clear_stats(GET_TEST_DATA(test)));
}


xmlNodePtr
recv_vst_rr_get_stats(test_t *test)
{
  return( vst_test_get_stats(test));
}

void
recv_vst_rr_decode_stats(xmlNodePtr stats,test_t *test)
{
  vst_test_decode_stats(stats,test);
}


int
send_vst_rr_clear_stats(test_t *test)
{
  return(vst_test_clear_stats(GET_TEST_DATA(test)));
}

xmlNodePtr
send_vst_rr_get_stats(test_t *test)
{
  return( vst_test_get_stats(test));
}

void
send_vst_rr_decode_stats(xmlNodePtr stats,test_t *test)
{
  vst_test_decode_stats(stats,test);
}


/* This routine implements the server-side of a TCP request/response */
/* test (a.k.a. rr) for the sockets interface. It receives its  */
/* parameters via the xml node contained in the test structure. */
/* The test looks at the first eight bytes of contents in the request */
/* to determine the actual size of the request and response messages. */
/* results are collected by the procedure recv_vst_rr_get_stats */

void
recv_vst_rr(test_t *test)
{
  uint32_t state, new_state;

  NETPERF_DEBUG_ENTRY(test->debug, test->where);

  vst_test_init(test, SOCK_STREAM, IPPROTO_TCP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      recv_vst_rr_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = recv_vst_rr_init(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        sleep(1);
      }
      break;
    case TEST_MEASURE:
      new_state = recv_vst_rr_meas(test);
      break;
    case TEST_LOADED:
      new_state = recv_vst_rr_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test,free_vst_test_data);

  NETPERF_DEBUG_EXIT(test->debug, test->where);

} /* end of recv_vst_rr */


/* This routine implements a TCP request/responce test */
/* (a.k.a. rr) for the sockets interface. It receives its */
/* parameters via the xml node contained in the test structure */
/* output to the standard output. The parameters include a pattern */
/* of variable sized requests and responces to send for the test */
/* results are collected by the procedure send_vst_rr_get_stats */

void
send_vst_rr(test_t *test)
{
  uint32_t state, new_state;
  vst_test_init(test, SOCK_STREAM, IPPROTO_TCP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      send_vst_rr_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = send_vst_rr_init(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        sleep(1);
      }
      break;
    case TEST_MEASURE:
      new_state = send_vst_rr_meas(test);
      break;
    case TEST_LOADED:
      new_state = send_vst_rr_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test,free_vst_test_data);
} /* end of send_vst_rr */


/*  This implementation of report_vst_test_results will generate strange
    results if transaction count and throughput tests are included in the
    same test set. The first test in the set sets the headers and algorithm
    for computing service demand */

static void
vst_test_results_init(tset_t *test_set,char *report_flags,char *output)
{
  vst_results_t *rd;
  FILE          *outfd;
  int            max_count;
  size_t         malloc_size;

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
  malloc_size = sizeof(vst_results_t) + 7 * max_count * sizeof(double);
  rd = malloc(malloc_size);
  if (rd) {

    /* original code took sizeof a math equation so memset only zeroed the */
    /* first sizeof(size_t) bytes.  This should work better  sgb 20060203  */

    memset(rd, 0, malloc_size);
    rd->max_count      = max_count;
    rd->results        = &(rd->results_start);
    rd->xmit_results   = &(rd->results[max_count]);
    rd->recv_results   = &(rd->xmit_results[max_count]);
    rd->trans_results  = &(rd->recv_results[max_count]);
    rd->utilization    = &(rd->trans_results[max_count]);
    rd->servdemand     = &(rd->utilization[max_count]);
    rd->run_time       = &(rd->servdemand[max_count]);
    rd->result_minimum = DBL_MAX;
    rd->result_maximum = DBL_MIN;
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
	rd->print_run  = 1;
	rd->print_test = 1;
      }
    }
    if (test_set->debug) {
      rd->print_run  = 1;
      rd->print_test = 1;
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
  vst_results_t *rd;

  double         elapsed_seconds;
  double         result;
  double         xmit_rate;
  double         recv_rate;
  double         xmit_trans_rate;
  double         recv_trans_rate;

#define TST_E_SEC         0
#define TST_E_USEC        1
#define TST_X_BYTES       5
#define TST_R_BYTES       7
#define TST_X_TRANS       8
#define TST_R_TRANS       9

#define MAX_TEST_CNTRS 12
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
    "cntr5_value",
    "cntr6_value",
    "cntr7_value"
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
        sscanf(value_str,"%"PRIx64,&x);
        test_cntr[i] = (double)x;
      }
    }
    else {
      test_cntr[i] = 0.0;
    }
    if (test_set->debug) {
      fprintf(test_set->where,"\t%12s test_cntr[%2d] = %10g\t'%s'\n",
              cntr_name[i], i, test_cntr[i],
              xmlGetProp(stats, (const xmlChar *)cntr_name[i]));
    }
  }
  elapsed_seconds = test_cntr[TST_E_SEC] + test_cntr[TST_E_USEC]/1000000.0;
  xmit_rate       = test_cntr[TST_X_BYTES]*8/(elapsed_seconds*1000000.0);
  recv_rate       = test_cntr[TST_R_BYTES]*8/(elapsed_seconds*1000000.0);
  xmit_trans_rate = test_cntr[TST_X_TRANS]/elapsed_seconds;
  recv_trans_rate = test_cntr[TST_R_TRANS]/elapsed_seconds;
  if (test_set->debug) {
    fprintf(test_set->where,"\txmit_rate = %7g\t%7g\n",
            xmit_rate, test_cntr[TST_X_BYTES]);
    fprintf(test_set->where,"\trecv_rate = %7g\t%7g\n",
            recv_rate, test_cntr[TST_R_BYTES]);
    fprintf(test_set->where,"\txmit_trans_rate = %7g\t%7g\n",
            xmit_trans_rate, test_cntr[TST_X_TRANS]);
    fprintf(test_set->where,"\trecv_trans_rate = %7g\t%7g\n",
            recv_trans_rate, test_cntr[TST_X_TRANS]);
    fflush(test_set->where);
  }
  if (rd->sd_denominator == 0.0) {
    if (xmit_trans_rate > 0.0 || recv_trans_rate > 0.0) {
      rd->sd_denominator = 1.0;
    }
    else {
      rd->sd_denominator = 1000000.0/(8.0*1024.0);
    }
  }
  if (test_set->debug) {
    fprintf(test_set->where,"\tsd_denominator = %f\n",rd->sd_denominator);
    fflush(test_set->where);
  }
  if (rd->sd_denominator != 1.0) {
    result = recv_rate + xmit_rate;
  }
  else {
    result = recv_trans_rate + xmit_trans_rate;
  }
  if (test_set->debug) {
    fprintf(test_set->where,"\tresult    = %f\n",result);
    fflush(test_set->where);
  }
  /* accumulate results for the run */
  rd->run_time[index]        += elapsed_seconds;
  rd->results[index]         += result;
  rd->xmit_results[index]    += xmit_rate;
  rd->recv_results[index]    += recv_rate;
  rd->trans_results[index]   += xmit_trans_rate;
  rd->trans_results[index]   += recv_trans_rate;

  if (rd->print_test) {
    /* Display per test results */
    fprintf(outfd,"%3d  ", count);                    /*  0,5 */
    fprintf(outfd,"%-6s ",  tid);                     /*  5,7 */
    fprintf(outfd,"%-6.2f ",elapsed_seconds);         /* 12,7 */
      fprintf(outfd,"%7.2f ",result);                 /* 19,8 */
      fprintf(outfd,"%7.2f ",xmit_rate);              /* 27,8 */
      fprintf(outfd,"%7.2f ",recv_rate);              /* 35,8 */
    fprintf(outfd,"\n");
    fflush(outfd);
  }
  /* end of printing vst per test instance results */
}

static double
process_sys_stats(tset_t *test_set, xmlNodePtr stats, xmlChar *tid)
{
  int            i;
  int            count;
  int            index;
  FILE          *outfd;
  vst_results_t *rd;
  double         elapsed_seconds;
  double         calibration;
  double         local_idle;
  double         local_busy;
  double         local_cpus;

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
        sscanf(value_str,"%"PRIx64,&x);
        sys_cntr[i] = (double)x;
      }
    }
    else {
      sys_cntr[i] = 0.0;
    }
    if (test_set->debug) {
      fprintf(test_set->where,"\t%12s sys_stats[%d] = %10g '%s'\n",
              sys_cntr_name[i], i, sys_cntr[i],
              xmlGetProp(stats, (const xmlChar *)sys_cntr_name[i]));
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
  vst_results_t *rd;
  test_t        *test;
  tset_elt_t    *set_elt;
  xmlNodePtr     stats;
  xmlNodePtr     prev_stats;
  int            count; 
  int            index;
  int            num_of_tests;
  double         num_of_cpus;
 

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

  rd->results[index]       =  0.0;
  rd->xmit_results[index]  =  0.0;
  rd->recv_results[index]  =  0.0;
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

  if (rd->result_minimum > rd->results[index]) {
    rd->result_minimum = rd->results[index];
  }
  if (rd->result_maximum < rd->results[index]) {
    rd->result_maximum = rd->results[index];
  }
  rd->run_time[index] = rd->run_time[index] / (double)num_of_tests;

  /* now calculate service demand for this test run. Remember the cpu */
  /* utilization is in the range 0.0 to 1.0 so we need to multiply by */
  /* the number of cpus and 1,000,000.0 to get to microseconds of cpu */
  /* time per unit of work.  The result is in transactions per second */
  /* or in million bits per second so the sd_denominator is factored  */
  /* in to convert service demand into usec/trans or usec/Kbytes.     */

  if ((rd->results[index] != 0.0) && (num_of_cpus != 0.0)) {
    rd->servdemand[index] = rd->utilization[index] * num_of_cpus * 1000000.0 /
                            (rd->results[index] * rd->sd_denominator);
  }
  NETPERF_DEBUG_EXIT(test_set->debug,test_set->where);
}

static void
update_results_and_confidence(tset_t *test_set)
{
  vst_results_t *rd;
  double         confidence;
  double         temp;
  int            loc_debug = 0;
  
  rd        = test_set->report_data;

  NETPERF_DEBUG_ENTRY(test_set->debug,test_set->where);

  /* calculate confidence and summary result values */
  confidence    = (test_set->get_confidence)(rd->xmit_results,
					     &(test_set->confidence),
					     &(rd->xmit_measured_mean),
					     &(rd->xmit_interval));
  if (test_set->debug || loc_debug) {
    fprintf(test_set->where, 
            "\txmit_results conf = %.2f%%\tmean = %10f +/- %8f\n",
            100.0 * confidence, rd->xmit_measured_mean, rd->xmit_interval);
    fflush(test_set->where);
  }

  confidence     = (test_set->get_confidence)(rd->recv_results,
					      &(test_set->confidence),
					      &(rd->recv_measured_mean),
					      &(rd->recv_interval));
  if (test_set->debug || loc_debug) {
    fprintf(test_set->where, 
            "\trecv_results conf = %.2f%%\tmean = %10f +/- %8f\n",
            100.0 * confidence, rd->recv_measured_mean, rd->recv_interval);
    fflush(test_set->where);
  }

  confidence      = (test_set->get_confidence)(rd->run_time,
					       &(test_set->confidence),
					       &(rd->ave_time),
					       &(temp));

  rd->result_confidence  = (test_set->get_confidence)(rd->results,
						      &(test_set->confidence),
						      &(rd->result_measured_mean),
						      &(rd->result_interval));
  if (test_set->debug || loc_debug) {
    fprintf(test_set->where, 
            "\tresults      conf = %.2f%%\tmean = %10f +/- %8f\n",
            100.0 * rd->result_confidence, 
            rd->result_measured_mean, rd->result_interval);
    fflush(test_set->where);
  }
  rd->cpu_util_confidence       = (test_set->get_confidence)(rd->utilization,
                                      &(test_set->confidence),
                                      &(rd->cpu_util_measured_mean),
                                      &(rd->cpu_util_interval));
  if (test_set->debug || loc_debug) {
    fprintf(test_set->where, 
            "\tcpu_util     conf = %.2f%%\tmean = %10f +/- %8f\n",
            100.0 * rd->cpu_util_confidence, 
            rd->cpu_util_measured_mean, rd->cpu_util_interval);
    fflush(test_set->where);
  }
  rd->service_demand_confidence = (test_set->get_confidence)(rd->servdemand,
                                      &(test_set->confidence),
                                      &(rd->service_demand_measured_mean),
                                      &(rd->service_demand_interval));
  if (test_set->debug || loc_debug) {
    fprintf(test_set->where, 
            "\tserv_demand  conf = %.2f%%\tmean = %10f +/- %8f\n",
            100.0 * rd->service_demand_confidence, 
            rd->service_demand_measured_mean, rd->service_demand_interval);
    fflush(test_set->where);
  }

  if (rd->result_confidence >  rd->cpu_util_confidence) {
    confidence = rd->result_confidence;
  }
  else {
    confidence = rd->cpu_util_confidence;
  }
  if (rd->service_demand_confidence > confidence) {
    confidence = rd->service_demand_confidence;
  }

  if (test_set->confidence.min_count > 1) {
    test_set->confidence.value = test_set->confidence.interval - confidence;
  }
  if (test_set->debug || loc_debug) {
    fprintf(test_set->where, 
            "\t%3drun confidence = %.2f%%\tcheck value = %f\n",
            test_set->confidence.count,
            100.0 * confidence, test_set->confidence.value);
    fflush(test_set->where);
  }
  NETPERF_DEBUG_EXIT(test_set->debug,test_set->where);
}

static void
print_run_results(tset_t *test_set)
{
  vst_results_t *rd;
  FILE          *outfd;
  int            i;
  int            count;
  int            index;

#define HDR_LINES 3
  char *field1[HDR_LINES]   = { "INST",  "NUM",  " "       };   /* 4 */
  char *field2[HDR_LINES]   = { "SET",   "Name", " "       };   /* 6 */
  char *field3[HDR_LINES]   = { "RUN",   "Time", "sec"     };   /* 6 */
  char *field4[HDR_LINES]   = { "TRANS", "RATE", "tran/s"  };   /* 7 */
  char *field5[HDR_LINES]   = { "XMIT",  "Rate", "mb/s"    };   /* 7 */
  char *field6[HDR_LINES]   = { "RECV",  "Rate", "mb/s"    };   /* 7 */
  char *field7[HDR_LINES]   = { "SD",    "usec", "/KB"     };   /* 7 */
  char *field8[HDR_LINES]   = { "CPU",   "Util", "%/100"   };   /* 6 */
#ifdef OFF
  char *field9[HDR_LINES]   = { "???",   "???",  "???"     };   /* 6 */
  char *field10[HDR_LINES]  = { "???",   "???",  "???"     };   /* 6 */
  char *field11[HDR_LINES]  = { "???",   "???",  "???"     };   /* 6 */
#endif

  rd    = test_set->report_data;
  count = test_set->confidence.count;
  outfd = rd->outfd;
  index = count - 1;
  

  /* Display per run header */
  fprintf(outfd,"\n");
  for (i=0;i < HDR_LINES; i++) {
    fprintf(outfd,"%-4s ",field1[i]);                         /*  0,5 */
    fprintf(outfd,"%-6s ",field2[i]);                         /*  5,7 */
    fprintf(outfd,"%-6s ",field3[i]);                         /* 12,7 */
      fprintf(outfd,"%7s ",field4[i]);                        /* 19,8 */
      fprintf(outfd,"%7s ",field5[i]);                        /* 27,8 */
      fprintf(outfd,"%7s ",field6[i]);                        /* 35,8 */
    fprintf(outfd,"%7s ",field7[i]);                          /* 43,8 */
    fprintf(outfd,"%6s ",field8[i]);                          /* 51,7 */
#ifdef OFF
    fprintf(outfd,"%6s ",field9[i]);                          /* 58,7 */
    fprintf(outfd,"%6s ",field10[i]);                         /* 65,7 */
    fprintf(outfd,"%6s ",field11[i]);                         /* 72,7 */
#endif
    fprintf(outfd,"\n");
  }
  
  /* Display per run results */
  fprintf(outfd,"%-3d  ", count);                             /*  0,5 */
  fprintf(outfd,"%-6s ",  test_set->id);                      /*  5,7 */
  fprintf(outfd,"%-6.2f ",rd->run_time[index]);               /* 12,7 */
    fprintf(outfd,"%7.2f ",rd->results[index]);               /* 19,8 */
    fprintf(outfd,"%7.2f ",rd->xmit_results[index]);          /* 27,8 */
    fprintf(outfd,"%7.2f ",rd->recv_results[index]);          /* 35,8 */
  fprintf(outfd,"%7.3f ",rd->servdemand[index]);              /* 43,8 */
  fprintf(outfd,"%6.4f ",rd->utilization[index]);             /* 51,7 */
#ifdef OFF
  fprintf(outfd,"%6.4f ",something to be added later);        /* 58,7 */
  fprintf(outfd,"%6.4f ",something to be added later);        /* 65,7 */
  fprintf(outfd,"%6.4f ",something to be added later);        /* 72,7 */
#endif
  fprintf(outfd,"\n");                                        /* 79,1 */
  fflush(outfd);
}


static void
print_did_not_meet_confidence(tset_t *test_set)
{
  vst_results_t *rd;
  FILE          *outfd;

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
          "!!! Confidence intervals: RESULT     : %6.2f%%\n",
          100.0 * rd->result_confidence);
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
  vst_results_t *rd;
  FILE          *outfd;
  int            i;

#define HDR_LINES 3
                                                                /* field
                                                                   width*/
  char *field1[HDR_LINES]   = { "AVE",   "Over", "Num"     };   /* 4 */
  char *field2[HDR_LINES]   = { "SET",   "Name", " "       };   /* 6 */
  char *field3[HDR_LINES]   = { "TEST",  "Time", "sec"     };   /* 6 */
  char *field4[HDR_LINES]   = { "TRANS", "RATE", "tran/s"  };   /* 7 */
  char *field5[HDR_LINES]   = { "conf",  "+/-",  "tran/s"  };   /* 7 */
  char *field6[HDR_LINES]   = { "XMIT",  "Rate", "mb/s"    };   /* 7 */
  char *field7[HDR_LINES]   = { "RECV",  "Rate", "mb/s"    };   /* 7 */
  char *field8[HDR_LINES]   = { "CPU",   "Util", "%/100"   };   /* 6 */
  char *field9[HDR_LINES]   = { "+/-",   "Util", "%/100"   };   /* 6 */
  char *field10[HDR_LINES]  = { "SD",    "usec", "/KB"     };   /* 6 */
  char *field11[HDR_LINES]  = { "+/-",   "usec", "/KB"     };   /* 6 */

  rd    = test_set->report_data;
  outfd = rd->outfd;

  /* Print the summary header */
  fprintf(outfd,"\n");
  for (i = 0; i < HDR_LINES; i++) {
    fprintf(outfd,"%-4s ",field1[i]);                             /*  0,5 */
    fprintf(outfd,"%-6s ",field2[i]);                             /*  5,7 */
    fprintf(outfd,"%-6s ",field3[i]);                             /* 12,7 */
    fprintf(outfd,"%7s ",field4[i]);                              /* 19,8 */
    fprintf(outfd,"%7s ",field5[i]);                              /* 27,8 */
    fprintf(outfd,"%7s ",field6[i]);                              /* 35,8 */
    fprintf(outfd,"%7s ",field7[i]);                              /* 43,8 */
    fprintf(outfd,"%6s ",field8[i]);                              /* 51,7 */
    fprintf(outfd,"%6s ",field9[i]);                              /* 58,7 */
    fprintf(outfd,"%6s ",field10[i]);                             /* 65,7 */
    fprintf(outfd,"%6s ",field11[i]);                             /* 72,7 */
    fprintf(outfd,"\n");
  }
  
  /* print the summary results line */
  fprintf(outfd,"A%-3d ",test_set->confidence.count);             /*  0,5 */
  fprintf(outfd,"%-6s ",test_set->id);                            /*  5,7 */
  fprintf(outfd,"%-6.2f ",rd->ave_time);                          /* 12,7 */
  fprintf(outfd,"%7.2f ",rd->result_measured_mean);               /* 19,8 */
  fprintf(outfd,"%7.3f ",rd->result_interval);                    /* 27,8 */
  fprintf(outfd,"%7.2f ",rd->xmit_measured_mean);                 /* 35,8 */
  fprintf(outfd,"%7.2f ",rd->recv_measured_mean);                 /* 43,8 */
  fprintf(outfd,"%6.4f ",rd->cpu_util_measured_mean);             /* 51,7 */
  fprintf(outfd,"%6.4f ",rd->cpu_util_interval);                  /* 58,7 */
  fprintf(outfd,"%6.3f ",rd->service_demand_measured_mean);       /* 65,7 */
  fprintf(outfd,"%6.3f ",rd->service_demand_interval);            /* 72,7 */
  fprintf(outfd,"\n");                                            /* 79,1 */
  fflush(outfd);
}

void
report_vst_test_results(tset_t *test_set, char *report_flags, char *output)
{
  vst_results_t *rd;
  int count;
  int max_count;
  int min_count;

  rd  = test_set->report_data;

  if (rd == NULL) {
    vst_test_results_init(test_set,report_flags,output);
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
} /* end of report_vst_test_results */

