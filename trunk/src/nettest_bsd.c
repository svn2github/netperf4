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
char    nettest_id[]="\
@(#)nettest_bsd.c (c) Copyright 2005 Hewlett-Packard Co. $Id$";
#else
#define DIRTY
#define WANT_HISTOGRAM
#endif /* lint */

#ifdef DIRTY
#define MAKE_DIRTY(mydata,ring)  /* DIRTY is not currently supported */
#else
#define MAKE_DIRTY(mydata,ring)  /* DIRTY is not currently supported */
#endif

/****************************************************************/
/*                                                              */
/*      nettest_bsd.c                                           */
/*                                                              */
/*      the actual test routines...                             */
/*                                                              */
/*      send_tcp_stream()       perform a tcp stream test       */
/*      recv_tcp_stream()       catch a tcp stream test         */
/*      send_tcp_rr()           perform a tcp req/rsp test      */
/*      recv_tcp_rr()           catch a tcp req/rsp test        */
/*      send_udp_stream()       perform a udp stream test       */
/*      recv_udp_stream()       catch a udp stream test         */
/*      send_udp_rr()           perform a udp req/rsp test      */
/*      recv_udp_rr()           catch a udp req/rsp test        */
/*      recv_tcp_rr_multi()     catch concurrent tcp req/rsp    */
/*                                tests                         */
/*                                                              */
/****************************************************************/



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

/* between the next three we aught to find DBL_MAX */
#ifdef HAVE_VALUES_H
#include <values.h>
#endif

#ifdef HAVE_MATH_H
#include <math.h>
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
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

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif

#include "netperf.h"
#include "netlib.h"

#include "nettest_bsd.h"

#include "netconfidence.h"

#ifdef WIN32
#define CHECK_FOR_INVALID_SOCKET (temp_socket == INVALID_SOCKET)
#define CHECK_FOR_RECV_ERROR(len) (len == SOCKET_ERROR)
#define GET_ERRNO WSAGetLastError()
#define CHECK_FOR_EADDRINUSE (GET_ERRNO == WSAEADDRINUSE)
#define CHECK_FOR_EADDRNOTAVAIL (GET_ERRNO == WSAEADDRNOTAVAIL)
#else
#define CHECK_FOR_INVALID_SOCKET (temp_socket < 0)
#define CHECK_FOR_RECV_ERROR(len) (len < 0)
#define GET_ERRNO errno
#define CHECK_FOR_EADDRINUSE (GET_ERRNO == EADDRINUSE)
#define CHECK_FOR_EADDRNOTAVAIL (GET_ERRNO == EADDRNOTAVAIL)
#endif


#if !defined(WANT_FIRST_BURST)
#define BURST_VARS            /* Burst variable declarations go here */
#define TCP_CHECK_BURST_SEND  /* Code to send tcp burst goes here */
#define UDP_CHECK_FOR_RETRANS /* Code to retrans udp packets goes here  */
#define UDP_CHECK_BURST_SEND  /* Code to send udp burst goes here */
#define UDP_CHECK_BURST_RECV  /* code for dropped udp packets goes here */
#else
#define TCP_CHECK_BURST_SEND \
    if (my_data->burst_count > my_data->pending_responses) { \
      my_data->pending_responses++; \
      my_data->send_ring = my_data->send_ring->next; \
      continue; \
    }
#define UDP_CHECK_FOR_RETRANS \
    if (my_data->retry_array != NULL) { \
      int retry; \
      retry = my_data->retry_index; \
      /* A burst size has been specified check if retransmission is needed */ \
      if (my_data->retry_array[retry] != NULL) { \
        /* a retransmission is needed a response was not received */ \
        buffer_ptr  = my_data->retry_array[retry]; \
        my_data->stats.named.retransmits++; \
      } \
      buffer_ptr[0] = retry; \
    }
#define UDP_CHECK_BURST_SEND \
    if (my_data->retry_array != NULL) { \
      int retry   = my_data->retry_index; \
      int pending = my_data->pending_responses; \
      int burst   = my_data->burst_count; \
      if (my_data->retry_array[retry] == NULL) { \
        my_data->retry_array[retry] = buffer_ptr; \
        retry++; \
        if (retry == burst) { \
          retry = 0; \
        } \
        pending++; \
        my_data->pending_responses = pending; \
        my_data->retry_index       = retry; \
        if (pending < burst) { \
          my_data->send_ring = my_data->send_ring->next; \
          continue; \
        } \
      } \
    }
#define UDP_CHECK_BURST_RECV \
    if (my_data->retry_array != NULL) { \
      my_data->retry_array[buffer_ptr[0]] = NULL; \
      my_data->pending_responses--; \
    }
#endif

#if !defined(WANT_SET_QUEUE)
  #define SET_QUEUE   /* multiqueue binding goes here */
#else
  #define SET_QUEUE \
    if (test) { \
      /* code to establish HPUX multiqueue binding */ \
      int          len; \
      bsd_data_t  *my_data; \
      my_data = GET_TEST_DATA(test); \
      if (my_data->multi_queue) { \
        if ((len=send(my_data->s_data, \
                      my_data->recv_ring->buffer_ptr, \
                      1, \
                      0)) != 1) { \
          /* this macro hides windows differences */ \
          if (CHECK_FOR_SEND_ERROR(len)) { \
            report_test_failure(test, \
                                (char *)__func__, \
                                BSDE_DATA_SEND_ERROR, \
                                "data send_error"); \
          } \
        } \
      } \
    }
#endif


static void
report_test_failure(test_t *test, const char *function, int err_code, const char * err_string) {
  if (test->debug) {
    fprintf(test->where,"%s: called report_test_failure:",function);
    fprintf(test->where,"reporting  %s  errno = %d\n",err_string,GET_ERRNO);
    fflush(test->where);
  }
  test->err_rc    = err_code;
  test->err_fn    = (char *)function;
  test->err_str   = (char *)err_string;
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
                             "set_test_state",
                             BSDE_REQUESTED_STATE_INVALID,
                             strdup(error_msg));
      }
    }
  }
}

static void
wait_to_die(test_t *test)
{
  while (GET_TEST_STATE != TEST_DEAD) {
    if (CHECK_REQ_STATE == TEST_DEAD) {
      free(GET_TEST_DATA(test));
      SET_TEST_DATA(test,NULL);
      SET_TEST_STATE(TEST_DEAD);
    }
  }
}





static int
get_port_number(struct addrinfo *res)
{
 int port;
 switch(res->ai_family) {
  case AF_INET: {
    struct sockaddr_in *foo = (struct sockaddr_in *)res->ai_addr;
    port = ntohs(foo->sin_port);
    break;
  }
#if defined(AF_INET6)
  case AF_INET6: {
    struct sockaddr_in6 *foo = (struct sockaddr_in6 *)res->ai_addr;
    port = ntohs(foo->sin6_port);
    break;
  }
#endif
  default:
    port = -1;
    break;
  }
  return (port);
}

/* this routine will set the port number of the sockaddr in the
   addrinfo to the specified value, based on the address family */
void
set_port_number(test_t *test, struct addrinfo *res, unsigned short port)
{
  switch(res->ai_family) {
  case AF_INET: {
    struct sockaddr_in *foo = (struct sockaddr_in *)res->ai_addr;
    foo->sin_port = htons(port);
    break;
  }
#if defined(AF_INET6)
  case AF_INET6: {
    struct sockaddr_in6 *foo = (struct sockaddr_in6 *)res->ai_addr;
    foo->sin6_port = htons(port);
    break;
  }
#endif
  default:
    fprintf(test->where,
            "%s: Unexpected Address Family of %u\n",
            __func__,
            res->ai_family);
    fflush(test->where);
  }
}


#ifdef notdef
static int
strtofam(xmlChar *familystr)
{
  if (!xmlStrcmp(familystr,(const xmlChar *)"AF_INET")) {
    return(AF_INET);
  } 
  else if (!xmlStrcmp(familystr,(const xmlChar *)"AF_UNSPEC")) {
    return(AF_UNSPEC);
#ifdef AF_INET6
  }
  else if (!xmlStrcmp(familystr,(const xmlChar *)"AF_INET6")) {
    return(AF_INET6);
#endif /* AF_INET6 */
  }
  else {
    /* we should never get here if the validator is doing its thing */
    return(-1);
  }
}
#endif


static void
get_dependency_data(test_t *test, int type, int protocol)
{
  bsd_data_t *my_data = GET_TEST_DATA(test);

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
      g_usleep(1000000);
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
                        BSDE_GETADDRINFO_ERROR,
                        gai_strerror(error));
  }
  if (string) free(string);
  if (remotehost) free(remotehost);
  if (remoteport) free(remoteport);
}


static void
set_dependent_data(test)
  test_t *test;
{
  
  bsd_data_t  *my_data = GET_TEST_DATA(test);
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
                          "set_dependent_data",
                          BSDE_XMLSETPROP_ERROR,
                          "error setting properties for dependency data");
    }
  } 
  else {
    report_test_failure(test,
                        "set_dependent_data",
                        BSDE_XMLNEWNODE_ERROR,
                        "error getting new node for dependency data");
  }
}


static unsigned int
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


 /* this routine will allocate a circular list of buffers for either */
 /* send or receive operations. each of these buffers will be aligned */
 /* and offset as per the users request. the circumference of this */
 /* ring will be controlled by the setting of send_width. the buffers */
 /* will be filled with data from the file specified in fill_file. if */
 /* fill_file is an empty string, the buffers will not be filled with */
 /* any particular data */

static ring_elt_ptr 
allocate_buffer_ring(width, buffer_size, alignment, offset, fill_source)
  int width;
  int buffer_size;
  int alignment;
  int offset;
  FILE *fill_source;
{
  ring_elt_ptr first_link = NULL;
  ring_elt_ptr temp_link  = NULL;
  ring_elt_ptr prev_link;

  int i;
  int malloc_size;
  int bytes_left;
  int bytes_read;
  int do_fill = 0;

  malloc_size = buffer_size + alignment + offset;

  /* did the user wish to have the buffers pre-filled with data from a */
  /* particular source? */
  if (fill_source != (FILE *)NULL) {
    do_fill = 1;
  }

  prev_link = NULL;
  for (i = 1; i <= width; i++) {
    /* get the ring element */
    temp_link = (struct ring_elt *)malloc(sizeof(struct ring_elt));
    /* remember the first one so we can close the ring at the end */
    if (i == 1) {
      first_link = temp_link;
    }
    temp_link->buffer_base = (char *)g_malloc(malloc_size);
#ifndef G_OS_WIN32
    temp_link->buffer_ptr = (char *)(( (long)(temp_link->buffer_base) +
                          (long)alignment - 1) &
                         ~((long)alignment - 1));
#else
    /* 64-bit Windows is P64, not LP64 like the rest of the world, 
       so we cannot cast as a "long" */
    temp_link->buffer_ptr = (char *)(( (ULONG_PTR)(temp_link->buffer_base) +
                          (long)alignment - 1) &
                         ~((long)alignment - 1));
#endif
    temp_link->buffer_ptr += offset;
    /* is where the buffer fill code goes. */
    if (do_fill) {
      bytes_left = buffer_size;
      while (bytes_left) {
        if (((bytes_read = fread(temp_link->buffer_ptr,
                                 1,
                                 bytes_left,
                                 fill_source)) == 0) &&
            (feof(fill_source))){
          rewind(fill_source);
        }
        bytes_left -= bytes_read;
      }
    }
    else {
      /* put our own special "stamp" upon the buffer */
      strncpy(temp_link->buffer_ptr,
	      NETPERF_RING_BUFFER_STRING,
	      buffer_size);
    }
    temp_link->next = prev_link;
    prev_link = temp_link;
  }
  first_link->next = temp_link;

  return(first_link); /* it's a circle, doesn't matter which we return */
}


 /* This routine will create a data (listen) socket with the apropriate */
 /* options set and return it to the caller. this replaces all the */
 /* duplicate code in each of the test routines and should help make */
 /* things a little easier to understand. since this routine can be */
 /* called by either the netperf or netserver programs, all output */
 /* should be directed towards "where." family is generally AF_INET, */
 /* and type will be either SOCK_STREAM or SOCK_DGRAM */
static SOCKET
create_data_socket(test)
  test_t *test;
{
  bsd_data_t  *my_data = GET_TEST_DATA(test);

  int family           = my_data->locaddr->ai_family;
  int type             = my_data->locaddr->ai_socktype;
  int lss_size         = my_data->send_buf_size;
  int lsr_size         = my_data->recv_buf_size;
  int loc_sndavoid     = my_data->send_avoid;
  int loc_rcvavoid     = my_data->recv_avoid;

  SOCKET temp_socket;
  int one;
  netperf_socklen_t sock_opt_len;

  if (test->debug) {
    fprintf(test->where,
            "%s:%s: calling socket family = %d type = %d\n",
	    __FILE__,
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
                        BSDE_SOCKET_ERROR,
                        "error creating socket");
    return(temp_socket);
  }

  if (test->debug) {
    fprintf(test->where,
            "%s:%s: socket %d obtained...\n",
	    __FILE__,
            __func__,
	    temp_socket);
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
                  (void *)&lss_size, sizeof(int)) < 0) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_SETSOCKOPT_ERROR,
                          "error setting local send socket buffer size");
      return(temp_socket);
    }
    if (test->debug > 1) {
      fprintf(test->where,
              "%s:%s: SO_SNDBUF of %d requested.\n",
	      __FILE__,
	      __func__,
              lss_size);
      fflush(test->where);
    }
  }

  if (lsr_size > 0) {
    if(setsockopt(temp_socket, SOL_SOCKET, SO_RCVBUF,
                  (void *)&lsr_size, sizeof(int)) < 0) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_SETSOCKOPT_ERROR,
                          "error setting local recv socket buffer size");
      return(temp_socket);
    }
    if (test->debug > 1) {
      fprintf(test->where,
              "%s: %s: SO_RCVBUF of %d requested.\n",
	      __FILE__,
              __func__, lsr_size);
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
	    __FILE__,
	    __func__, GET_ERRNO);
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
	    __FILE__,
	    __func__, GET_ERRNO);
    fflush(test->where);
    lsr_size = -1;
  }
  if (test->debug) {
    fprintf(test->where,
            "%s: %s: socket sizes determined...\n",
	    __FILE__,
            __func__);
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
	      __FILE__,
              __func__);
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
	      __FILE__,
              __func__);
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
	      __FILE__,
              __func__, GET_ERRNO);
      fflush(test->where);
    }

    if (test->debug > 1) {
      fprintf(test->where,
              "netperf: %s: TCP_NODELAY requested...\n", __func__);
      fflush(test->where);
    }
  }
#else /* TCP_NODELAY */

  my_data->no_delay = 0;

#endif /* TCP_NODELAY */

  return(temp_socket);

}



bsd_data_t *
bsd_test_init(test_t *test, int type, int protocol)
{
  bsd_data_t *new_data;
  xmlNodePtr  args;
  xmlChar    *string;
  xmlChar    *test_name;
  xmlChar    *units;
  xmlChar    *localhost;
  xmlChar    *localport;
  int         localfam;

  int               count;
  int               error;
  struct addrinfo   hints;
  struct addrinfo  *local_ai;

  /* allocate memory to store the information about this test */
  new_data = (bsd_data_t *)malloc(sizeof(bsd_data_t));

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
    /* zero the bsd test specific data structure */
    memset(new_data,0,sizeof(bsd_data_t));

    string =  xmlGetProp(args,(const xmlChar *)"fill_file");
    /* fopen the fill file it will be used when allocating buffer rings */
    if (string) {
      new_data->fill_source = fopen((char *)string,"r");
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

    /* PN: 07/31/2007
     * Number of concurrent connections this test should handle
     * (such as number of send_tcp_rr tests in case of a recv_tcp_rr_multi
     * test). This number makes sense (or should make sense) only for
     * multi tests.
     *
     * We need some kind of validation to ensure "num_concurrent_conns"
     * is not > 0 for non-multi tests, since, a lot memory allocation
     * depends on the value of "num_concurrent_conns". Right now, we use
     * the test name for validation.
     */
    string =  xmlGetProp(args,(const xmlChar *)"num_concurrent_conns");
    test_name = xmlGetProp(test->node,(const xmlChar *)"test_name");
    if (!xmlStrcmp(test_name, (const xmlChar *)"recv_tcp_rr_multi")) {
      new_data->num_concurrent_conns = atoi((char *)string);

      /* num_concurrent_conns should be > 0 */
      if (!(new_data->num_concurrent_conns))
              new_data->num_concurrent_conns = 1;
    }
    else {
      new_data->num_concurrent_conns = 0;
    }
    if (test->debug) {
      fprintf(test->where, "Test: %s, num_concurrent_conns: %d\n",
                (char *)test_name, new_data->num_concurrent_conns);
      fflush(test->where);
    }

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
      new_data->port_max = -1;
    }

    string =  xmlGetProp(args,(const xmlChar *)"multi_queue");
    if (string) {
      new_data->multi_queue = atoi((char *)string);
    }
    else {
      new_data->multi_queue = 0;
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

    string =  xmlGetProp(args,(const xmlChar *)"burst_count");
    if (string) {
      count = atoi((char *)string);
      new_data->burst_count = count;
      if (count >= new_data->send_width) {
        new_data->send_width  = count + 1;
      }
      string = xmlGetProp(test->node,(const xmlChar *)"test_name");
      if (!xmlStrcmp(string, (const xmlChar *)"send_udp_rr")) {
        new_data->retry_array = malloc(count * sizeof(char *));
        if (new_data->retry_array) {
          memset(new_data->retry_array, 0, count * sizeof(char *));
        }
      }
    }

    string =  xmlGetProp(args,(const xmlChar *)"interval_time");
    if (string) {
      new_data->interval    = atoi((char *)string);
    }

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
        g_usleep(1000000);
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
                          BSDE_GETADDRINFO_ERROR,
                          gai_strerror(error));
    }
  }
  else {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_NO_SOCKET_ARGS,
                        "no socket_arg element was found");
  }

  /* PN: 07/31/2007
   * If this test should handle concurrent connections, allocate memory
   * for helper threads' data.  Helper threads are used by multi tests
   * such as recv_tcp_rr_multi.
   */
  if (new_data->num_concurrent_conns) {
    new_data->helper_data = (generic_helper_data_t *)
        malloc(sizeof(generic_helper_data_t) * new_data->num_concurrent_conns);
    memset(new_data->helper_data, 0,
         sizeof(generic_helper_data_t) * new_data->num_concurrent_conns);
  }

  SET_TEST_DATA(test,new_data);
  return(new_data);
}

static void
update_elapsed_time(bsd_data_t *my_data)
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
bsd_test_clear_stats(bsd_data_t *my_data)
{
  int i;
  for (i = 0; i < BSD_MAX_COUNTERS; i++) {
    my_data->stats.counter[i] = 0;
  }
  my_data->elapsed_time.tv_usec = 0;
  my_data->elapsed_time.tv_sec  = 0;
  gettimeofday(&(my_data->prev_time),NULL);
  my_data->curr_time = my_data->prev_time;
  return(NPE_SUCCESS);
}

void
bsd_test_decode_stats(xmlNodePtr stats,test_t *test)
{
  if (test->debug) {
    fprintf(test->where,"%s: entered for %s test %s\n",__func__,
            test->id, test->test_name);
    fflush(test->where);
  }
}

static xmlNodePtr
bsd_test_get_stats(test_t *test)
{
  xmlNodePtr  stats = NULL;
  xmlAttrPtr  ap    = NULL;
  int         i;
  char        value[32];
  char        name[32];
  uint64_t    loc_cnt[BSD_MAX_COUNTERS];

  bsd_data_t *my_data = GET_TEST_DATA(test);

  if (test->debug) {
    fprintf(test->where,
	    "%s: entered for %s test %s\n",
	    __func__,
            test->id, test->test_name);
    fflush(test->where);
  }
  if ((stats = xmlNewNode(NULL,(xmlChar *)"test_stats")) != NULL) {
    /* set the properites of the test_stats message -
       the tid and time stamps/values and counter values  sgb 2004-07-07 */

    ap = xmlSetProp(stats,(xmlChar *)"tid",test->id);
    for (i = 0; i < BSD_MAX_COUNTERS; i++) {
      loc_cnt[i] = my_data->stats.counter[i];
      if (test->debug) {
        fprintf(test->where,"BSD_COUNTER%X = %#"PRIx64"\n",i,loc_cnt[i]);
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
    for (i = 0; i < BSD_MAX_COUNTERS; i++) {
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
    fprintf(test->where,
	    "%s: exiting for %s test %s\n",
	    __func__,
            test->id,
	    test->test_name);
    fflush(test->where);
  }
  return(stats);
} /* end of bsd_test_get_stats */


static void
recv_tcp_stream_preinit(test_t *test)
{
  int               rc;         
  SOCKET           s_listen;
  bsd_data_t       *my_data;
  struct sockaddr   myaddr;
  netperf_socklen_t mylen;

  my_data   = GET_TEST_DATA(test);
  mylen     = sizeof(myaddr);

  my_data->recv_ring = allocate_buffer_ring(my_data->recv_width,
                                            my_data->recv_size,
                                            my_data->recv_align,
                                            my_data->recv_offset,
                                            my_data->fill_source);
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
            __func__, rc, GET_ERRNO);
    fflush(test->where);
  }
  if (rc == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_BIND_FAILED,
                        "data socket bind failed");
  }
  else if (listen(s_listen,5) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_LISTEN_FAILED,
                        "data socket listen failed");
  }
  else if (getsockname(s_listen,&myaddr,&mylen) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_GETSOCKNAME_FAILED,
                        "getting the listen socket name failed");
  }
  else {
    memcpy(my_data->locaddr->ai_addr,&myaddr,mylen);
    my_data->locaddr->ai_addrlen = mylen;
    set_dependent_data(test);
  }
}

static uint32_t
recv_tcp_stream_init(test_t *test)
{
  SOCKET            s_data;
  bsd_data_t       *my_data;
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
                        BSDE_ACCEPT_FAILED,
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
recv_tcp_stream_idle_link(test_t *test, int last_len)
{
  uint32_t          new_state;
  bsd_data_t       *my_data;
  struct sockaddr   peeraddr;
  netperf_socklen_t peerlen;
  int               len;

  my_data   = GET_TEST_DATA(test);
  peerlen   = sizeof(peeraddr);
  len       = last_len;

  while (len > 0) {
    if ((len=recv(my_data->s_data,
                  my_data->recv_ring->buffer_ptr,
                  my_data->recv_size,
                  0)) != 0) {
      /* this macro hides windows differences */
      if (CHECK_FOR_RECV_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_RECV_ERROR,
                            "data_recv_error");
      }
    }
  } 

  new_state = CHECK_REQ_STATE;
  if (test->debug) {
    fprintf(test->where,"**** %s:new_state = %d\n",__func__,new_state);
    fflush(test->where);
  }
  while (new_state == TEST_LOADED) {
    g_usleep(1000000);
    new_state = CHECK_REQ_STATE;
  }
  if (test->debug) {
    fprintf(test->where,"**** %s:new_state = %d\n",__func__,new_state);
    fflush(test->where);
  }
  if (new_state == TEST_IDLE) {
    if (shutdown(my_data->s_data,SHUT_WR) == -1) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_SOCKET_SHUTDOWN_FAILED,
                          "failure shuting down data socket");
    }
    else {
      CLOSE_SOCKET(my_data->s_data);
      if (test->debug) {
        fprintf(test->where,"%s: waiting in accept\n",__func__);
        fflush(test->where);
      }
      if ((my_data->s_data=accept(my_data->s_listen,
                                 &peeraddr,
                                 &peerlen)) == -1) {
        report_test_failure(test,
                          (char *)__func__,
                          BSDE_ACCEPT_FAILED,
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
                        BSDE_DATA_CONNECTION_CLOSED_ERROR,
                        "data connection closed and non idle state requested");

  }
}

static uint32_t
recv_tcp_stream_meas(test_t *test)
{
  int               len;
  uint32_t          new_state;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  HISTOGRAM_VARS;
  /* code to make data dirty macro enabled by DIRTY */
  MAKE_DIRTY(my_data, my_data->recv_ring);
  /* code to timestamp enabled by WANT_HISTOGRAM */
  HIST_TIMESTAMP(&time_one);
  /* recv data for the test */
  if ((len=recv(my_data->s_data,
                my_data->recv_ring->buffer_ptr,
                my_data->recv_size,
                0)) != 0) {
    /* this macro hides windows differences */
    if (CHECK_FOR_RECV_ERROR(len)) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_RECV_ERROR,
                          "data_recv_error");
    }
    else {
      my_data->stats.named.bytes_received += len;
      my_data->stats.named.recv_calls++;
      my_data->recv_ring = my_data->recv_ring->next;
    }
  }
  else {
    /* how do we deal with a closed connection in the loaded state */
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_DATA_CONNECTION_CLOSED_ERROR,
                        "data connection closed during TEST_MEASURE state");
  }
  /* code to timestamp enabled by WANT_HISTOGRAM */
  HIST_TIMESTAMP(&time_two);
  HIST_ADD(my_data->time_hist,&time_one,&time_two);
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
recv_tcp_stream_load(test_t *test)
{
  int               len;
  uint32_t          new_state;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  /* code to make data dirty macro enabled by DIRTY */
  MAKE_DIRTY(my_data, my_data->recv_ring);
  /* recv data for the test */
  if ((len=recv(my_data->s_data,
                my_data->recv_ring->buffer_ptr,
                my_data->recv_size,
                0)) != 0) {
    /* this macro hides windows differences */
    if (CHECK_FOR_RECV_ERROR(len)) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_RECV_ERROR,
                          "data_recv_error");
    }
    else {
      my_data->recv_ring = my_data->recv_ring->next;
    }
  }
  /* check for state transition */
  new_state = CHECK_REQ_STATE;
  if ((len == 0) || 
      (new_state == TEST_IDLE)) {
    /* just got a data connection close or
       a request to transition to the idle state */
    recv_tcp_stream_idle_link(test,len);
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
send_tcp_stream_preinit(test_t *test)
{
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  if (my_data->s_data == 0) {
    my_data->send_ring = allocate_buffer_ring(my_data->send_width,
                                              my_data->send_size,
                                              my_data->send_align,
                                              my_data->send_offset,
                                              my_data->fill_source);
    get_dependency_data(test, SOCK_STREAM, IPPROTO_TCP);
    my_data->s_data = create_data_socket(test);
  }
  else {
    fprintf(test->where,"entered %s more than once\n",
	    __func__);
    fflush(test->where);
  }
}

static uint32_t
send_tcp_stream_init(test_t *test)
{
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  if (test->debug) {
    fprintf(test->where,"%s: in INIT state making connect call\n",__func__);
    fflush(test->where);
  }
  if (connect(my_data->s_data,
              my_data->remaddr->ai_addr,
              my_data->remaddr->ai_addrlen) < 0) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_CONNECT_FAILED,
                        "data socket connect failed");
  }
  else {
    if (test->debug) {
      fprintf(test->where,"%s: connected and moving to IDLE\n",__func__);
      fflush(test->where);
    }
  }
  return(TEST_IDLE);
}

static void
send_tcp_stream_idle_link(test_t *test)
{
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  if (test->debug) {
    fprintf(test->where,"%s: transition from LOAD to IDLE\n",__func__);
    fflush(test->where);
  }
  if (shutdown(my_data->s_data,SHUT_WR) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_SOCKET_SHUTDOWN_FAILED,
                        "failure shuting down data socket");
  }
  else {
    recv(my_data->s_data,
         my_data->send_ring->buffer_ptr,
         my_data->send_size, 0);
    CLOSE_SOCKET(my_data->s_data);
    my_data->s_data = create_data_socket(test);
    if (test->debug) {
      fprintf(test->where,"%s: connecting from LOAD state\n",__func__);
      fflush(test->where);
    }
    if (connect(my_data->s_data,
                my_data->remaddr->ai_addr,
                my_data->remaddr->ai_addrlen) < 0) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_CONNECT_FAILED,
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

static uint32_t
send_tcp_stream_meas(test_t *test)
{
  int               len;
  uint32_t          new_state;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  HISTOGRAM_VARS;
  /* code to make data dirty macro enabled by DIRTY */
  MAKE_DIRTY(my_data,my_data->send_ring);
  /* code to timestamp enabled by WANT_HISTOGRAM */
  HIST_TIMESTAMP(&time_one);
  /* send data for the test */
  if((len=send(my_data->s_data,
               my_data->send_ring->buffer_ptr,
               my_data->send_size,
               0)) != my_data->send_size) {
    /* this macro hides windows differences */
    if (CHECK_FOR_SEND_ERROR(len)) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_SEND_ERROR,
                          "data send error");
    }
  }
  my_data->stats.named.bytes_sent += len;
  my_data->stats.named.send_calls++;
  my_data->send_ring = my_data->send_ring->next;
  /* code to timestamp enabled by WANT_HISTOGRAM */
  HIST_TIMESTAMP(&time_two);
  HIST_ADD(my_data->time_hist,&time_one,&time_two);
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
send_tcp_stream_load(test_t *test)
{
  int               len;
  uint32_t          new_state;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  HISTOGRAM_VARS;
  /* code to make data dirty macro enabled by DIRTY */
  MAKE_DIRTY(my_data,my_data->send_ring);
  /* code to timestamp enabled by WANT_HISTOGRAM */
  HIST_TIMESTAMP(&time_one);
  /* send data for the test */
  if((len=send(my_data->s_data,
               my_data->send_ring->buffer_ptr,
               my_data->send_size,
               0)) != my_data->send_size) {
    /* this macro hides windows differences */
    if (CHECK_FOR_SEND_ERROR(len)) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_SEND_ERROR,
                          "data send_error");
    }
  }
  my_data->send_ring = my_data->send_ring->next;
  /* check for state transition */
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_MEASURE) {
    /* transitioning to measure state from loaded state
       set previous timestamp */
    gettimeofday(&(my_data->prev_time), NULL);
  }
  if (new_state == TEST_IDLE) {
    send_tcp_stream_idle_link(test);
  }
  return(new_state);
}

int
recv_tcp_stream_clear_stats(test_t *test)
{
  return(bsd_test_clear_stats(GET_TEST_DATA(test)));
}


xmlNodePtr
recv_tcp_stream_get_stats(test_t *test)
{
  return( bsd_test_get_stats(test));
}

void
recv_tcp_stream_decode_stats(xmlNodePtr stats,test_t *test)
{
  bsd_test_decode_stats(stats,test);
}


int
send_tcp_stream_clear_stats(test_t *test)
{
  return(bsd_test_clear_stats(GET_TEST_DATA(test)));
}

xmlNodePtr
send_tcp_stream_get_stats(test_t *test)
{
  return( bsd_test_get_stats(test));
}

void
send_tcp_stream_decode_stats(xmlNodePtr stats,test_t *test)
{
  bsd_test_decode_stats(stats,test);
}


/* This routine implements the server-side of the TCP unidirectional data */
/* transfer test (a.k.a. stream) for the sockets interface. It receives */
/* its parameters via the xml node contained in the test structure. */
/* results are collected by the procedure recv_tcp_stream_get_stats */

void
recv_tcp_stream(test_t *test)
{
  uint32_t state, new_state;
  bsd_test_init(test, SOCK_STREAM, IPPROTO_TCP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      recv_tcp_stream_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = recv_tcp_stream_init(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        g_usleep(1000000);
      }
      if (new_state == TEST_LOADED) {
        SET_QUEUE;
      }
      break;
    case TEST_MEASURE:
      new_state = recv_tcp_stream_meas(test);
      break;
    case TEST_LOADED:
      new_state = recv_tcp_stream_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
} /* end of recv_tcp_stream */


/* This routine implements the TCP unidirectional data transfer test */
/* (a.k.a. stream) for the sockets interface. It receives its */
/* parameters via the xml node contained in the test structure */
/* output to the standard output. */
/* results are collected by the procedure send_tcp_stream_get_stats */

void
send_tcp_stream(test_t *test)
{
  uint32_t state, new_state;
  bsd_test_init(test, SOCK_STREAM, IPPROTO_TCP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      send_tcp_stream_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = send_tcp_stream_init(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        g_usleep(1000000);
      }
      break;
    case TEST_MEASURE:
      new_state = send_tcp_stream_meas(test);
      break;
    case TEST_LOADED:
      new_state = send_tcp_stream_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
}  /* end of send_tcp_stream */


static void
recv_tcp_rr_preinit(test_t *test)
{
  int               rc;
  SOCKET           s_listen;
  bsd_data_t       *my_data;
  struct sockaddr   myaddr;
  netperf_socklen_t mylen;

  my_data   = GET_TEST_DATA(test);

  mylen     = sizeof(myaddr);

  my_data->recv_ring = allocate_buffer_ring(my_data->recv_width,
                                            my_data->req_size,
                                            my_data->recv_align,
                                            my_data->recv_offset,
                                            my_data->fill_source);
  my_data->send_ring = allocate_buffer_ring(my_data->send_width,
                                            my_data->rsp_size,
                                            my_data->send_align,
                                            my_data->send_offset,
                                            my_data->fill_source);
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
            __func__, rc, GET_ERRNO);
    fflush(test->where);
  }
  if (rc == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_BIND_FAILED,
                        "data socket bind failed");
  } 
  else if (listen(s_listen,5) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_LISTEN_FAILED,
                        "data socket listen failed");
  }
  else if (getsockname(s_listen,&myaddr,&mylen) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_GETSOCKNAME_FAILED,
                        "getting the listen socket name failed");
  } 
  else {
    memcpy(my_data->locaddr->ai_addr,&myaddr,mylen);
    my_data->locaddr->ai_addrlen = mylen;
    set_dependent_data(test);
  }
}

static uint32_t
recv_tcp_rr_init(test_t *test)
{
  SOCKET             s_data;
  bsd_data_t       *my_data;
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
                        BSDE_ACCEPT_FAILED,
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
recv_tcp_rr_idle_link(test_t *test, int last_len)
{
  int               len;
  uint32_t          new_state;
  bsd_data_t       *my_data;
  struct sockaddr   peeraddr;
  netperf_socklen_t peerlen;

  my_data   = GET_TEST_DATA(test);

  len       = last_len;
  peerlen   = sizeof(peeraddr);

  new_state = CHECK_REQ_STATE;
  while (new_state == TEST_LOADED) {
    g_usleep(1000000);
    new_state = CHECK_REQ_STATE;
  }

  if (new_state == TEST_IDLE) {
    if (test->debug) {
      fprintf(test->where,"%s: transition from LOAD to IDLE\n",__func__);
      fflush(test->where);
    }
    if (shutdown(my_data->s_data,SHUT_WR) == -1) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_SOCKET_SHUTDOWN_FAILED,
                          "data_recv_error");
    } 
    else {
      while (len > 0) {
        len=recv(my_data->s_data,
                 my_data->recv_ring->buffer_ptr,
                 my_data->req_size, 0);
      }
      CLOSE_SOCKET(my_data->s_data);
      if (test->debug) {
        fprintf(test->where,"%s: waiting in accept\n",__func__);
        fflush(test->where);
      }
      if ((my_data->s_data=accept(my_data->s_listen,
                                 &peeraddr,
                                 &peerlen)) == -1) {
        report_test_failure(test,
                          (char *)__func__,
                          BSDE_ACCEPT_FAILED,
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
                        BSDE_DATA_CONNECTION_CLOSED_ERROR,
                        "data connection closed and non idle state requested");
  }
}

static uint32_t
recv_tcp_rr_meas(test_t *test)
{
  int               len = -1;
  int               bytes_left;
  char             *req_ptr;
  uint32_t          new_state;
  bsd_data_t       *my_data;

  HISTOGRAM_VARS;
  my_data   = GET_TEST_DATA(test);

  /* code to timestamp enabled by WANT_HISTOGRAM */
  HIST_TIMESTAMP(&time_one);
  /* recv the request for the test */
  req_ptr    = my_data->recv_ring->buffer_ptr;
  bytes_left = my_data->req_size;
  while (bytes_left > 0) {
    if ((len=recv(my_data->s_data,
                  req_ptr,
                  bytes_left,
                  0)) != 0) {
      /* this macro hides windows differences */
      if (CHECK_FOR_RECV_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_RECV_ERROR,
                            "data_recv_error");
        break;
      }
      req_ptr    += len;
      bytes_left -= len;
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
                        BSDE_DATA_CONNECTION_CLOSED_ERROR,
                        "data connection closed during TEST_MEASURE state");
  }
  else {
    my_data->stats.named.trans_received++;
    if ((len=send(my_data->s_data,
                  my_data->send_ring->buffer_ptr,
                  my_data->rsp_size,
                  0)) != my_data->rsp_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data_send_error");
      }
    }
  }
  /* code to timestamp enabled by WANT_HISTOGRAM */
  HIST_TIMESTAMP(&time_two);
  HIST_ADD(my_data->time_hist,&time_one,&time_two);
  my_data->recv_ring = my_data->recv_ring->next;
  my_data->send_ring = my_data->send_ring->next;
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
recv_tcp_rr_load(test_t *test)
{
  int               len=-1;
  int               bytes_left;
  char             *req_ptr;
  uint32_t          new_state;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  /* recv the request for the test */
  req_ptr    = my_data->recv_ring->buffer_ptr;
  bytes_left = my_data->req_size;
  while (bytes_left > 0) {
    if ((len=recv(my_data->s_data,
                  req_ptr,
                  bytes_left,
                  0)) != 0) {
      /* this macro hides windows differences */
      if (CHECK_FOR_RECV_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_RECV_ERROR,
                            "data_recv_error");
        break;
      }
      req_ptr    += len;
      bytes_left -= len;
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
    recv_tcp_rr_idle_link(test,len);
    new_state = TEST_IDLE;
  }
  else {
    if ((len=send(my_data->s_data,
                  my_data->send_ring->buffer_ptr,
                  my_data->rsp_size,
                  0)) != my_data->rsp_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data_send_error");
      }
    }
    my_data->recv_ring = my_data->recv_ring->next;
    my_data->send_ring = my_data->send_ring->next;
    if (new_state == TEST_MEASURE) {
      /* transitioning to measure state from loaded state
         set previous timestamp */
      gettimeofday(&(my_data->prev_time), NULL);
    }
  } 
  return(new_state);
}

/* PN: 07/31/2007
 * 'Multi' tests such as recv_tcp_rr_multi spawn helper threads
 * to handle concurrent senders.
 * This function handles load/meas for recv_tcp_rr_multi's helper threads
 */
static uint32_t
recv_tcp_rr_multi_helper_run(generic_helper_data_t *my_data)
{
  int               len=-1;
  int               bytes_left;
  char             *req_ptr;
  int               rsp_size, req_size, err;

  test_t           *test = my_data->test_being_helped;

  /* recv the request for the test */
  req_ptr    = my_data->recv_ring->buffer_ptr;
  req_size   = bytes_left = ((bsd_data_t *)
        GET_TEST_DATA(my_data->test_being_helped))->req_size;

  while (bytes_left > 0) {
    if ((len=recv(my_data->s_data,
                  req_ptr,
                  bytes_left,
                  0)) != 0) {
      /* this macro hides windows differences */
      if (CHECK_FOR_RECV_ERROR(len)) {
        report_test_failure(my_data->test_being_helped,
                            (char *)__func__,
                            BSDE_DATA_RECV_ERROR,
                            "data_recv_error");
        break;
      }
      req_ptr    += len;
      bytes_left -= len;
    }
    else {
      /* just got a data connection close break out of while loop */
      break;
    }
  }

  /* check for recv len and state transition */
  if ((len == 0) ||
      (my_data->state_req == TEST_IDLE)) {
    /* just got a data connection close or
       a request to transition to the idle state */
    /* shut down the socket and set it to -1 */
    if (test->debug) {
      if (len == 0)
        fprintf(test->where,
          "%s: Attempting socket shutdown: socket=%d recv len=0 of bytes_left=%d\n",
                __func__, my_data->s_data, bytes_left);
      if (my_data->state_req == TEST_IDLE)
        fprintf(test->where,
          "%s: Attempting socket shutdown: socket=%dtransition to IDLE state\n",
                __func__, my_data->s_data);
      fflush(test->where);
    }

    if (shutdown(my_data->s_data,SHUT_WR) == -1) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_SOCKET_SHUTDOWN_FAILED,
                          "data_recv_error");


    }
    else {
      while (len > 0) {
        len=recv(my_data->s_data,
                 my_data->recv_ring->buffer_ptr,
                 req_size, 0);
      }
      CLOSE_SOCKET(my_data->s_data);
      my_data->s_data = -1;
    }

  }
  else {

    /* Increment counter if we are in measuring state */
    if (my_data->new_state == TEST_MEASURE) my_data->stats_counter++;

    /* Send the response */
    rsp_size = ((bsd_data_t *)GET_TEST_DATA(my_data->test_being_helped))->rsp_size;
    if ((len=send(my_data->s_data,
                  my_data->send_ring->buffer_ptr,
                  rsp_size,
                  0)) != rsp_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(my_data->test_being_helped,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data_send_error");
      }
    }
    my_data->recv_ring = my_data->recv_ring->next;
    my_data->send_ring = my_data->send_ring->next;

  }

  return (my_data->state_req);

}

/* PN: 07/31/2007
 * 'Multi' tests such as recv_tcp_rr_multi spawn helper threads
 * to handle concurrent senders.
 * This function handles state transitions, and is the starting point
 * for recv_tcp_rr_multi's helper threads.
 */
static void *
recv_tcp_rr_multi_helper(void *helper_data)
{
  uint32_t state, new_state;
  generic_helper_data_t *my_data = (generic_helper_data_t *)helper_data;
  test_t *test = my_data->test_being_helped;

  state = my_data->new_state;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {

    switch(state) {
    case TEST_IDLE:
      new_state = my_data->state_req;
      if (new_state == TEST_IDLE) {
        g_usleep(1000000);
      }
      break;
    case TEST_LOADED:
      new_state = recv_tcp_rr_multi_helper_run(my_data);
      if (new_state == TEST_IDLE) {
        /* Transition from load to idle.
         * Ensure helper_run shutdowns/closes the socket
         */
        while (my_data->s_data > 0)
          new_state = recv_tcp_rr_multi_helper_run(my_data);
      }
      else if (my_data->s_data < 0) {
        /* Did not recv() any data in the previous
         * recv_tcp_rr_multi_helper_run. Most likely the peer closed
         * its socket. Sleep until the test_being_helped changes my
         * state to TEST_IDLE (that is when test_being_helped creates
         * me a new socket.
         */
        while ((new_state = my_data->state_req) != TEST_IDLE)
          g_usleep(100000);
      }
      break;
    case TEST_MEASURE:
      new_state = recv_tcp_rr_multi_helper_run(my_data);
      break;
    default:
        /* TODO. Do not know to handle this state!!!! */
      break;
    } /* end of switch */
    /* Not necessary to check if state transitions are correct,
     * that will be taken care by the recv_tcp_rr_multi test
     */
    state = my_data->new_state = new_state;
  } /* end of while */

  if (test->debug) {
    fprintf(test->where, "%s: thread: %p (helper for test: %s) will exit now\n",
        __func__, my_data->tid, (char *)test->id);
    fflush(test->where);
  }

  /* This thread is all set to exit */
  g_thread_exit(NULL);

} /* end of recv_tcp_rr_multi_helper */

/* PN: 07/31/2007
 * 'Multi' tests such as recv_tcp_rr_multi spawn helper threads
 * to handle concurrent senders.
 * This function handles launching helper threads for recv_tcp_rr_multi test.
 */
static uint32_t
recv_tcp_rr_multi_init(test_t *test)
{
  SOCKET                 s_data;
  bsd_data_t            *my_data;
  struct sockaddr        peeraddr;
  netperf_socklen_t      peerlen;
  int                    num_accepted = 0;
  generic_helper_data_t *temp = NULL;
  uint32_t               ret_val = TEST_IDLE;
  int                    rc;

  my_data   = GET_TEST_DATA(test);

  peerlen   = sizeof(peeraddr);

  NETPERF_DEBUG_ENTRY(test->debug, test->where);

  /* Accept num_concurrent_conns senders */
  while (num_accepted < my_data->num_concurrent_conns) {
    if (test->debug) {
      fprintf(test->where, "%s: waiting to accept sender: %d\n",
        __func__, num_accepted);
      fflush(test->where);
    }

    temp                        = (generic_helper_data_t *)
                                        &(my_data->helper_data[num_accepted]);

    /* All helper threads send and recv from the same buffer.
     * This can be changed if needed.
     */
    temp->send_ring             = my_data->send_ring;
    temp->recv_ring             = my_data->recv_ring;
    temp->state_req             = TEST_IDLE;
    temp->new_state             = TEST_IDLE;
    temp->test_being_helped     = test;

    if ((temp->s_data = accept(my_data->s_listen,
                      &peeraddr,
                      &peerlen)) == -1) {
      report_test_failure(test,
                        (char *)__func__,
                        BSDE_ACCEPT_FAILED,
                        "listen socket accept failed");
      ret_val = TEST_ERROR;
      break;
    }
    else {
      if (test->debug) {
        fprintf(test->where,
              "%s: accept returned successfully %d\n",
              __func__, temp->s_data);
        fflush(test->where);
      }
    }

    rc = launch_joinable_thread(&(temp->tid), recv_tcp_rr_multi_helper,
                    (void *)temp);

    if (rc != NPE_SUCCESS) {
        test->state = TEST_ERROR;
        test->err_rc = rc;
        test->err_fn = (char *)__func__;
        ret_val = TEST_ERROR;
        if (test->debug) {
          fprintf(test->where,
              "%s: launch_joinable_thread failed for sender: %d\n",
              __func__, num_accepted);
          fflush(test->where);
        }
        break;
    }
    else {
        if (test->debug) {
          fprintf(test->where,
              "%s: launch_joinable_thread successful. new thread: %p\n",
              __func__, temp->tid);
          fflush(test->where);
        }
    }
    num_accepted++;
  }

  NETPERF_DEBUG_EXIT(test->debug, test->where);
  return(ret_val);
}

/* PN: 07/31/2007
 * 'Multi' tests such as recv_tcp_rr_multi spawn helper threads
 * to handle concurrent senders.
 * This function handles load to idle state transition for recv_tcp_rr_multi.
 */
static void
recv_tcp_rr_multi_idle_link(uint32_t from_state, uint32_t to_state, test_t *test)
{

  int                    num_accepted = 0;
  generic_helper_data_t *temp = NULL;
  uint32_t               ret_val = TEST_IDLE;
  int                    rc;
  bsd_data_t            *my_data;
  int                    num_helpers, i;
  struct sockaddr        peeraddr;
  netperf_socklen_t      peerlen;

  my_data               = GET_TEST_DATA(test);
  num_helpers           = my_data->num_concurrent_conns;
  peerlen               = sizeof(peeraddr);

  NETPERF_DEBUG_ENTRY(test->debug, test->where);

  if (from_state == TEST_LOADED && to_state == TEST_IDLE) {
    /* transitioning from load to idle. */

    /* All helper threads should shutdown/close their socket
     * and set sock fd to -1. Wail until this happens
     */
    for (i=0; i<num_helpers; i++) {
      while (my_data->helper_data[i].s_data > 0)
        g_usleep(1000000);
    }

    /* Wait on accept */
    if (test->debug) {
      fprintf(test->where, "%s:waiting in accept\n", __func__);
      fflush(test->where);
    }
    while (num_accepted < my_data->num_concurrent_conns) {

      temp                      = (generic_helper_data_t *)
                                        &(my_data->helper_data[num_accepted]);

      temp->state_req           = TEST_IDLE;
      temp->new_state           = TEST_IDLE;

      if ((temp->s_data = accept(my_data->s_listen,
                      &peeraddr,
                      &peerlen)) == -1) {
        report_test_failure(test,
                        (char *)__func__,
                        BSDE_ACCEPT_FAILED,
                        "listen socket accept failed");
        ret_val = TEST_ERROR;
        break;
      }
      else {
        if (test->debug) {
          fprintf(test->where,
              "%s:accept returned successfully %d\n",
              __func__, temp->s_data);
          fflush(test->where);
        }
      }
      num_accepted++;

    } /* end while */

  } /* End of state transition load->idle */

  NETPERF_DEBUG_EXIT(test->debug, test->where);

}

/* PN: 07/31/2007
 * 'Multi' tests such as recv_tcp_rr_multi spawn helper threads
 * to handle concurrent senders.
 * This function has been designed to be a "generic" multi test
 * transition helper. I.e., the transition logic is not tied to any specific
 * multi test. The transition helper assists a multi test as follows:
 * 1. Set requested state for all helper threads to to_state
 * 2. Take appropriate action based on state transtion:
 *   - LOAD to IDLE => call the multi test's idle_link_fn, which is an
 *     input parameter.
 *   - LOAD to MEAS => set multi test's prev timestamp.
 *   - MEAS to LOAD => set multi test's curr timestamp, update elapsed time.
 *   - DEAD => wait for all helper threads to join (exit)
 */
static void
transition_helper_threads(test_t *test, uint32_t to_state,
        void (*idle_link_fn)(uint32_t, uint32_t, test_t *))
{

  bsd_data_t    *my_data;
  int            num_helpers, i;
  uint32_t       from_state, new_state;
  my_data       = GET_TEST_DATA(test);

  num_helpers   = my_data->num_concurrent_conns;

  NETPERF_DEBUG_ENTRY(test->debug, test->where);

  /* Get the current state of the helpers */
  from_state = my_data->helper_data[0].new_state;

  if (test->debug) {
    fprintf(test->where, "%s: test: %s state %d to state %d\n",
        __func__, (char *)test->id,from_state, to_state);
    fflush(stdout);
  }

  for (i=0; i<num_helpers; i++) {
    my_data->helper_data[i].state_req = to_state;
  }

  if (to_state == TEST_DEAD) {
    /* Wait until all helpers exit */
    for (i=0; i<num_helpers; i++) {
        if (test->debug) {
          fprintf(test->where, "%s: test: %s waiting on thread %p to join\n",
                __func__, (char *)test->id, my_data->helper_data[i]);
        }
        g_thread_join(my_data->helper_data[i].tid);
    }
  }
  else {
    /* Wait until all helpers transition state */
    for (i=0; i<num_helpers; i++) {
       while (my_data->helper_data[i].new_state != to_state) {
         if (test->debug) {
           fprintf(test->where, "%s: test: %s waiting for thread %p to change state\n",
                     __func__, (char *)test->id, my_data->helper_data[i]);
           fflush(test->where);
          }
         g_usleep(500000);
       }
     }
  }

  if (from_state == TEST_LOADED && to_state == TEST_IDLE) {
    /* transitioning to load to idle. */
    idle_link_fn(from_state, to_state, test);
    /* At this point, all helpers's sock fds should be closed
     * and everyone is idle
     */
  }

  if (from_state == TEST_LOADED && to_state == TEST_MEASURE) {
    /* transitioning to measure from load. set previous timestamp */
    gettimeofday(&(my_data->prev_time), NULL);
  }

  if (from_state == TEST_MEASURE && to_state == TEST_LOADED) {
   /* transitioning to load from measure. set current timestamp
    * and upadte elapsed time
    */
    gettimeofday(&(my_data->curr_time), NULL);
    update_elapsed_time(my_data);
  }

  NETPERF_DEBUG_EXIT(test->debug, test->where);

}

static void
send_tcp_rr_preinit(test_t *test)
{
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  my_data->recv_ring = allocate_buffer_ring(my_data->recv_width,
                                            my_data->rsp_size,
                                            my_data->recv_align,
                                            my_data->recv_offset,
                                            my_data->fill_source);
  my_data->send_ring = allocate_buffer_ring(my_data->send_width,
                                            my_data->req_size,
                                            my_data->send_align,
                                            my_data->send_offset,
                                            my_data->fill_source);

  get_dependency_data(test, SOCK_STREAM, IPPROTO_TCP);
  my_data->s_data = create_data_socket(test);
}

static uint32_t
send_tcp_rr_init(test_t *test)
{
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  if (test->debug) {
    fprintf(test->where,"%s: in INIT state making connect call\n",__func__);
    fflush(test->where);
  }
  if (connect(my_data->s_data,
              my_data->remaddr->ai_addr,
              my_data->remaddr->ai_addrlen) < 0) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_CONNECT_FAILED,
                        "data socket connect failed");
  }
  else {
    if (test->debug) {
      fprintf(test->where,"%s: connected and moving to IDLE\n",__func__);
      fflush(test->where);
    }
  }
  return(TEST_IDLE);
}

static void
send_tcp_rr_idle_link(test_t *test, int last_len)
{
  int               len;
  uint32_t          new_state;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);
  len       = last_len;

  new_state = CHECK_REQ_STATE;
  while (new_state == TEST_LOADED) {
    g_usleep(1000000);
    new_state = CHECK_REQ_STATE;
  }
  if (new_state == TEST_IDLE) {
    if (test->debug) {
      fprintf(test->where,"%s: transition from LOAD to IDLE\n",__func__);
      fflush(test->where);
    }
    if (shutdown(my_data->s_data,SHUT_WR) == -1) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_SOCKET_SHUTDOWN_FAILED,
                          "failure shuting down data socket");
    } 
    else {
      while (len > 0) {
        len = recv(my_data->s_data,
                   my_data->recv_ring->buffer_ptr,
                   my_data->rsp_size, 0);
      }
      CLOSE_SOCKET(my_data->s_data);
      my_data->s_data = create_data_socket(test);
      if (test->debug) {
        fprintf(test->where,"%s: connecting from LOAD state\n",__func__);
        fflush(test->where);
      }
      if (connect(my_data->s_data,
                  my_data->remaddr->ai_addr,
                  my_data->remaddr->ai_addrlen) < 0) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_CONNECT_FAILED,
                            "data socket connect failed");
      }
      else {
        if (test->debug) {
          fprintf(test->where,"%s: connected moving to IDLE\n",__func__);
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
                        BSDE_DATA_CONNECTION_CLOSED_ERROR,
                        "data connection closed and non idle state requested");

  }
}

static uint32_t
send_tcp_rr_meas(test_t *test)
{
  uint32_t          new_state;
  int               len;
  int               bytes_left;
  char             *rsp_ptr;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  while (NO_STATE_CHANGE(test)) {
    HISTOGRAM_VARS;
    /* code to make data dirty macro enabled by DIRTY */
    MAKE_DIRTY(my_data,my_data->send_ring);
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_one);
    /* send data for the test */
    if((len=send(my_data->s_data,
                 my_data->send_ring->buffer_ptr,
                 my_data->req_size,
                 0)) != my_data->req_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data send error");
      }
    }
    TCP_CHECK_BURST_SEND;
    /* recv the request for the test */
    rsp_ptr    = my_data->recv_ring->buffer_ptr;
    bytes_left = my_data->rsp_size;
    while (bytes_left > 0) {
      if ((len=recv(my_data->s_data,
                    rsp_ptr,
                    bytes_left,
                    0)) != 0) {
        /* this macro hides windows differences */
        if (CHECK_FOR_RECV_ERROR(len)) {
          report_test_failure(test,
                              (char *)__func__,
                              BSDE_DATA_RECV_ERROR,
                              "data_recv_error");
          break;
        }
        rsp_ptr    += len;
        bytes_left -= len;
      } else {
        /* len is 0 the connection was closed exit the while loop */
        break;
      }
    }
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_two);
    HIST_ADD(my_data->time_hist,&time_one,&time_two);
    my_data->stats.named.trans_sent++;
    my_data->recv_ring = my_data->recv_ring->next;
    my_data->send_ring = my_data->send_ring->next;
    if (len == 0) {
      /* how do we deal with a closed connection in the measured state */
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_CONNECTION_CLOSED_ERROR,
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
send_tcp_rr_load(test_t *test)
{
  uint32_t          new_state;
  int               len;
  int               bytes_left;
  char             *rsp_ptr;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  while (NO_STATE_CHANGE(test)) {
    /* code to make data dirty macro enabled by DIRTY */
    MAKE_DIRTY(my_data,my_data->send_ring);
    /* send data for the test */
    if((len=send(my_data->s_data,
                 my_data->send_ring->buffer_ptr,
                 my_data->req_size,
                 0)) != my_data->req_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data send error");
      }
    }
    TCP_CHECK_BURST_SEND;
    /* recv the request for the test */
    rsp_ptr    = my_data->recv_ring->buffer_ptr;
    bytes_left = my_data->rsp_size;
    while (bytes_left > 0) {
      if ((len=recv(my_data->s_data,
                    rsp_ptr,
                    bytes_left,
                    0)) != 0) {
        /* this macro hides windows differences */
        if (CHECK_FOR_RECV_ERROR(len)) {
          report_test_failure(test,
                              (char *)__func__,
                              BSDE_DATA_RECV_ERROR,
                              "data_recv_error");
          break;
        }
        rsp_ptr    += len;
        bytes_left -= len;
      } else {
        /* len is 0 the connection was closed exit the while loop */
        break;
      }
    }
    my_data->recv_ring = my_data->recv_ring->next;
    my_data->send_ring = my_data->send_ring->next;
    if (len == 0) {
      /* len is 0 the connection was closed exit the while loop */
      break;
    }
  }
  new_state = CHECK_REQ_STATE;
  if ((len == 0) ||
      (new_state == TEST_IDLE)) {
    send_tcp_rr_idle_link(test,len);
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
recv_tcp_rr_clear_stats(test_t *test)
{
  return(bsd_test_clear_stats(GET_TEST_DATA(test)));
}


xmlNodePtr
recv_tcp_rr_get_stats(test_t *test)
{
  return( bsd_test_get_stats(test));
}

void
recv_tcp_rr_decode_stats(xmlNodePtr stats,test_t *test)
{
  bsd_test_decode_stats(stats,test);
}

int
recv_tcp_rr_multi_clear_stats(test_t *test)
{

  /* Clear helper threads' stats_counter */
  int i, num_helpers;
  generic_helper_data_t *helper_data =
        ((bsd_data_t *)GET_TEST_DATA(test))->helper_data;
  num_helpers = ((bsd_data_t *)GET_TEST_DATA(test))->num_concurrent_conns;

  for (i=0; i<num_helpers; i++)
    helper_data[i].stats_counter = 0;

  return(bsd_test_clear_stats(GET_TEST_DATA(test)));
}

xmlNodePtr
recv_tcp_rr_multi_get_stats(test_t *test)
{

  /* Gather stats from all helper threads. */
  int i, num_helpers;
  bsd_data_t *my_data = (bsd_data_t *)GET_TEST_DATA(test);
  generic_helper_data_t *helper_data = my_data->helper_data;
  num_helpers = my_data->num_concurrent_conns;

  /* recv_tcp_rr_multi helpers track trans_received. */
  for (i=0; i<num_helpers; i++)
    my_data->stats.named.trans_received += helper_data[i].stats_counter;

  return( bsd_test_get_stats(test));
}

void
recv_tcp_rr_multi_decode_stats(xmlNodePtr stats,test_t *test)
{
  bsd_test_decode_stats(stats,test);
}

int
send_tcp_rr_clear_stats(test_t *test)
{
  return(bsd_test_clear_stats(GET_TEST_DATA(test)));
}

xmlNodePtr
send_tcp_rr_get_stats(test_t *test)
{
  return( bsd_test_get_stats(test));
}

void
send_tcp_rr_decode_stats(xmlNodePtr stats,test_t *test)
{
  bsd_test_decode_stats(stats,test);
}


/* This routine implements the server-side of the TCP request/response */
/* test (a.k.a. rr) for the sockets interface. It receives its  */
/* parameters via the xml node contained in the test structure. */
/* results are collected by the procedure recv_tcp_rr_get_stats */

void
recv_tcp_rr(test_t *test)
{
  uint32_t state, new_state;
  bsd_test_init(test, SOCK_STREAM, IPPROTO_TCP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      recv_tcp_rr_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = recv_tcp_rr_init(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        g_usleep(1000000);
      }
      break;
    case TEST_MEASURE:
      new_state = recv_tcp_rr_meas(test);
      break;
    case TEST_LOADED:
      new_state = recv_tcp_rr_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
} /* end of recv_tcp_rr */


/* PN: 07/31/2007
 * A 'multi' test such as recv_tcp_rr_multi handles multiple concurrent
 * concurrent send_tcp_rr tests. The number of senders to handle is determined
 * by "num_concurrent_conns" of socket_args.
 *
 * The test does the following:
 *   - Accepts() num_concurrent_conns senders.
 *   - For each accepted connection, spawn a new "helper" thread to handle
 *     the data transfer.
 *   - The netperf client talks only to the main recv_tcp_rr_multi thread.
 *   - Based on state transition requests from the netperf client, the main
 *     thread transitions the helper threads.
 *   - Each helper thread has an associated generic_helper_data structure, and
 *     the helper thread writes/reads members of this structure.
 *   - Helper threads increment the stats_counter member of generic_helper_data
 *     in MEAS state.
 *   - recv_tcp_rr_multi_get_stats sums up all helper threads' stats_counter
 *     before calling bsd_get_stats.
 */
void
recv_tcp_rr_multi(test_t *test)
{
  uint32_t state, new_state;
  int transition_done = 0;

  /* Define recv_tcp_rr_multi's idle link function to be used in
   * transtion_helper_threads(). */
  void (*my_idle_link_fn)(uint32_t, uint32_t, test_t *)
         = recv_tcp_rr_multi_idle_link;

  NETPERF_DEBUG_ENTRY(test->debug, test->where);

  bsd_test_init(test, SOCK_STREAM, IPPROTO_TCP);

  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      /* recv_tcp_rr_multi preinit is identical to recv_tcp_rr_preinit,
       * so why dont we just call recv_tcp_rr_preinit..
       */
      recv_tcp_rr_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = recv_tcp_rr_multi_init(test);
      }
      break;
    case TEST_IDLE:
     if (!transition_done) {
        transition_helper_threads(test, TEST_IDLE, my_idle_link_fn);
        transition_done = 1;
     }
     new_state = CHECK_REQ_STATE;
     if (new_state != state) transition_done = 0;
     if (new_state == TEST_IDLE) {
        g_usleep(1000000);
     }
     break;
    case TEST_MEASURE:
      if (!transition_done) {
        transition_helper_threads(test, TEST_MEASURE, my_idle_link_fn);
        transition_done = 1;
     }
     new_state = CHECK_REQ_STATE;
     if (new_state != state) transition_done = 0;
      break;
    case TEST_LOADED:
      if (!transition_done) {
        transition_helper_threads(test, TEST_LOADED, my_idle_link_fn);
        transition_done = 1;
     }
     new_state = CHECK_REQ_STATE;
     if (new_state != state) transition_done = 0;
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */

  if (state == TEST_DEAD || state == TEST_ERROR) {
    /* Transition all helpers to the dead state, and wait
     * until all helpers exit */
    transition_helper_threads(test, TEST_DEAD, my_idle_link_fn);
  }

  /* All helper threads have joined. Free helper data */
  free(((bsd_data_t *)GET_TEST_DATA(test))->helper_data);

  wait_to_die(test);

  NETPERF_DEBUG_EXIT(test->debug, test->where);

} /* end of recv_tcp_rr_multi */


/* This routine implements the TCP request/response test */
/* (a.k.a. rr) for the sockets interface. It receives its */
/* parameters via the xml node contained in the test structure */
/* output to the standard output. */
/* results are collected by the procedure send_tcp_rr_get_stats */

void
send_tcp_rr(test_t *test)
{
  uint32_t state, new_state;
  bsd_test_init(test, SOCK_STREAM, IPPROTO_TCP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      send_tcp_rr_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = send_tcp_rr_init(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        g_usleep(1000000);
      }
      break;
    case TEST_MEASURE:
      new_state = send_tcp_rr_meas(test);
      break;
    case TEST_LOADED:
      new_state = send_tcp_rr_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
} /* end of send_tcp_rr */



#define BSD_TCP_CRR_TESTS
#ifdef BSD_TCP_CRR_TESTS
static void
recv_tcp_crr_preinit(test_t *test)
{
  int               rc;
  SOCKET           s_listen;
  bsd_data_t       *my_data;
  struct sockaddr   myaddr;
  netperf_socklen_t mylen;

  my_data   = GET_TEST_DATA(test);

  mylen     = sizeof(myaddr);

  my_data->recv_ring = allocate_buffer_ring(my_data->recv_width,
                                            my_data->req_size,
                                            my_data->recv_align,
                                            my_data->recv_offset,
                                            my_data->fill_source);
  my_data->send_ring = allocate_buffer_ring(my_data->send_width,
                                            my_data->rsp_size,
                                            my_data->send_align,
                                            my_data->send_offset,
                                            my_data->fill_source);
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
            __func__, rc, GET_ERRNO);
    fflush(test->where);
  }
  if (rc == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_BIND_FAILED,
                        "data socket bind failed");
  } 
  else if (listen(s_listen,5) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_LISTEN_FAILED,
                        "data socket listen failed");
  }
  else if (getsockname(s_listen,&myaddr,&mylen) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_GETSOCKNAME_FAILED,
                        "getting the listen socket name failed");
  } 
  else {
    memcpy(my_data->locaddr->ai_addr,&myaddr,mylen);
    my_data->locaddr->ai_addrlen = mylen;
    set_dependent_data(test);
  }
}

static uint32_t
recv_tcp_crr_init(test_t *test)
{
  SOCKET             s_data;
  bsd_data_t       *my_data;
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
                        BSDE_ACCEPT_FAILED,
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


static uint32_t
recv_tcp_crr_meas(test_t *test)
{
  SOCKET             s_data;
  int                len = -1;
  int                bytes_left;
  char              *req_ptr;
  uint32_t           new_state;
  bsd_data_t        *my_data;
  struct sockaddr    peeraddr;
  netperf_socklen_t  peerlen;


  HISTOGRAM_VARS;
  my_data   = GET_TEST_DATA(test);


  while (NO_STATE_CHANGE(test)) {
    /* accept a connection from the remote */
    if ((s_data = accept(my_data->s_listen,
                        &peeraddr,
                        &peerlen)) == -1) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_ACCEPT_FAILED,
                          "listen socket accept failed");
      continue;
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
    /* receive the request from the other side */
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_one);
    /* recv the request for the test */
    req_ptr    = my_data->recv_ring->buffer_ptr;
    bytes_left = my_data->req_size;
    while (bytes_left > 0) {
      len=recv(s_data,
               req_ptr,
               bytes_left,
               0);
      if (len > 0) {
        req_ptr    += len;
        bytes_left -= len;
        continue;
      }
      else {
        break;
      }
    }
    /* this macro hides windows differences */
    if (CHECK_FOR_RECV_ERROR(len)) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_RECV_ERROR,
                          "data_recv_error");
      continue;
    }
    if (len == 0) {
      /* just got a data connection close break out of while loop */
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_CONNECTION_CLOSED_ERROR,
                          "data_connection_closed_early");
      continue;
    }
    /* Now, send the response to the remote */
    if ((len=send(s_data,
                  my_data->send_ring->buffer_ptr,
                  my_data->rsp_size,
                  0)) != my_data->rsp_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data_send_error");
        continue;
      }
    }
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_two);
    HIST_ADD(my_data->time_hist,&time_one,&time_two);
    my_data->stats.named.trans_received++;
    my_data->recv_ring = my_data->recv_ring->next;
    my_data->send_ring = my_data->send_ring->next;
    /* close the connection. the server will likely do a graceful */
    /* close of the connection, insuring that all data has arrived at */
    /* the client. for this it will call shutdown(), and then recv() and */
    /* then close(). I'm reasonably confident that this is the */
    /* appropriate sequence of calls - I would like to hear of */
    /* examples in web servers to the contrary. raj 10/95*/
#ifdef TCP_CRR_SHUTDOWN
    shutdown(s_data, SHUT_WR);
    recv(s_data,
         recv_message_ptr,
         1,
         0);
    close(s_data);
#else
    close(s_data);
#endif /* TCP_CRR_SHUTDOWN */
  }
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_LOADED) {
    /* transitioning to loaded state from measure state
       set current timestamp and update elapsed time */
    gettimeofday(&(my_data->curr_time), NULL);
    update_elapsed_time(my_data);
    my_data->s_data = -1;
  }
  return(new_state);
}

static uint32_t
recv_tcp_crr_load(test_t *test)
{
  SOCKET             s_data;
  int                len=-1;
  int                bytes_left;
  char              *req_ptr;
  uint32_t           new_state;
  bsd_data_t        *my_data;
  struct sockaddr    peeraddr;
  netperf_socklen_t  peerlen;
  int                ready;
  fd_set             readfds;
  struct timeval     timeout;


  my_data   = GET_TEST_DATA(test);

  FD_ZERO(&readfds);
  FD_SET(my_data->s_listen, &readfds);
  timeout.tv_sec  = 1;
  timeout.tv_usec = 0;

  s_data    = my_data->s_data;
  while (NO_STATE_CHANGE(test)) {
    if (s_data == -1) {
      /* do a select call to ensure that the accept will not block    */
      /* if the accept blocks the test may hang on state transitions  */
      ready = select(my_data->s_listen+1, &readfds, NULL, NULL, &timeout);
      if (ready == 0) {
        FD_SET(my_data->s_listen, &readfds);
        continue;
      }
      /* accept a connection from the remote */
      if ((s_data = accept(my_data->s_listen,
                          &peeraddr,
                          &peerlen)) == -1) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_ACCEPT_FAILED,
                            "listen socket accept failed");
        continue;
      }
      else {
        if (test->debug) {
          fprintf(test->where,
                  "%s:accept returned successfully %d\n",
                  __func__, s_data);
          fflush(test->where);
        }
      }
    }
    /* receive the request from the other side */
    req_ptr    = my_data->recv_ring->buffer_ptr;
    bytes_left = my_data->req_size;
    while (bytes_left > 0) {
      len=recv(s_data,
               req_ptr,
               bytes_left,
               0);
      if (len > 0) {
        req_ptr    += len;
        bytes_left -= len;
        continue;
      }
      else {
        break;
      }
    }
    /* this macro hides windows differences */
    if (CHECK_FOR_RECV_ERROR(len)) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_RECV_ERROR,
                          "data_recv_error");
      continue;
    }
    if (len == 0) {
      /* just got a data connection close break out of while loop */
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_CONNECTION_CLOSED_ERROR,
                          "data_connection_closed_early");
      continue;
    }
    /* Now, send the response to the remote */
    if ((len=send(s_data,
                  my_data->send_ring->buffer_ptr,
                  my_data->rsp_size,
                  0)) != my_data->rsp_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data_send_error");
        continue;
      }
    }
    my_data->recv_ring = my_data->recv_ring->next;
    my_data->send_ring = my_data->send_ring->next;
    /* close the connection. the server will likely do a graceful */
    /* close of the connection, insuring that all data has arrived at */
    /* the client. for this it will call shutdown(), and then recv() and */
    /* then close(). I'm reasonably confident that this is the */
    /* appropriate sequence of calls - I would like to hear of */
    /* examples in web servers to the contrary. raj 10/95*/
#ifdef TCP_CRR_SHUTDOWN
    shutdown(s_data, SHUT_WR);
    recv(s_data,
         recv_message_ptr,
         1,
         0);
    close(s_data);
#else
    close(s_data);
#endif /* TCP_CRR_SHUTDOWN */
    s_data = -1;
  }
  /* check for state transition */
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_MEASURE) {
    /* transitioning to measure state from loaded state
       set previous timestamp */
    gettimeofday(&(my_data->prev_time), NULL);
  } 
  return(new_state);
}


static void
send_tcp_crr_preinit(test_t *test)
{
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  if (my_data->port_min == -1) {
    my_data->port_min = 5000;
  }

  if (my_data->port_max == -1) {
    my_data->port_max = 65535;
  }

  my_data->recv_ring = allocate_buffer_ring(my_data->recv_width,
                                            my_data->rsp_size,
                                            my_data->recv_align,
                                            my_data->recv_offset,
                                            my_data->fill_source);
  my_data->send_ring = allocate_buffer_ring(my_data->send_width,
                                            my_data->req_size,
                                            my_data->send_align,
                                            my_data->send_offset,
                                            my_data->fill_source);

  get_dependency_data(test, SOCK_STREAM, IPPROTO_TCP);
  my_data->s_data = create_data_socket(test);
}

static uint32_t
send_tcp_crr_init(test_t *test)
{
  bsd_data_t       *my_data;
  int               myport;
  int               rc;

  my_data   = GET_TEST_DATA(test);

  if (test->debug) {
    fprintf(test->where,"%s: in INIT state making connect call\n",__func__);
    fflush(test->where);
  }

  myport = my_data->port_min;
  srand(getpid());
  if (my_data->port_max - my_data->port_min) {
    myport = my_data->port_min + 
             (rand() % (my_data->port_max - my_data->port_min));
  }
  for (;;) {
    my_data->s_data = create_data_socket(test);
    myport++;
    if (myport >= my_data->port_max) {
      myport = my_data->port_min;
    }
    set_port_number(test, my_data->locaddr, myport);
    rc = bind(my_data->s_data,
              my_data->locaddr->ai_addr,
              my_data->locaddr->ai_addrlen);
    if (rc < 0) {
      if (!(CHECK_FOR_EADDRINUSE || CHECK_FOR_EADDRNOTAVAIL)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_BIND_FAILED,
                            "data socket bind failed");
        
        break;
      }
      close(my_data->s_data);
      my_data->s_data = -1;
      continue;
    }
    else {
      rc = connect(my_data->s_data,
                   my_data->remaddr->ai_addr,
                   my_data->remaddr->ai_addrlen);
      if (rc < 0) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_CONNECT_FAILED,
                            "data socket connect failed");
        close(my_data->s_data);
        my_data->s_data = -1;
      }
      else if (test->debug) {
        fprintf(test->where,"%s: connected and moving to IDLE\n",__func__);
        fflush(test->where);
      }
      break;
    }
  }
  return(TEST_IDLE);
}


static uint32_t
send_tcp_crr_meas(test_t *test)
{
  uint32_t          new_state;
  int               bytes_left;
  int               len;
  int               myport;
  int               rc;
  char             *rsp_ptr;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  myport    = get_port_number(my_data->locaddr);

  while (NO_STATE_CHANGE(test)) {
    HISTOGRAM_VARS;
    /* code to make data dirty macro enabled by DIRTY */
    MAKE_DIRTY(my_data,my_data->send_ring);
    /* code to timestamp enabled by WANT_HISTOGRAM */
    /* change the port number and get a new socket */
    my_data->s_data = create_data_socket(test);
    myport++;
    if (myport >= my_data->port_max) {
      myport = my_data->port_min;
    }
    set_port_number(test, my_data->locaddr, myport);
    rc = bind(my_data->s_data,
              my_data->locaddr->ai_addr, 
              my_data->locaddr->ai_addrlen);
    if (rc < 0) {
      if (!(CHECK_FOR_EADDRINUSE || CHECK_FOR_EADDRNOTAVAIL)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_BIND_FAILED,
                            "data socket bind failed");
      }
      continue;
    }
    HIST_TIMESTAMP(&time_one);
    if (connect(my_data->s_data,
                my_data->remaddr->ai_addr,
                my_data->remaddr->ai_addrlen) < 0) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_CONNECT_FAILED,
                          "data socket connect failed");
      continue;
    }
    /* send data for the test */
    if((len=send(my_data->s_data,
                 my_data->send_ring->buffer_ptr,
                 my_data->req_size,
                 0)) != my_data->req_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                           (char *)__func__,
                           BSDE_DATA_SEND_ERROR,
                           "data send error");
        continue;
      }
    }
    /* recv the request for the test */
    rsp_ptr    = my_data->recv_ring->buffer_ptr;
    bytes_left = my_data->rsp_size;
    while (bytes_left > 0) {
      len = recv(my_data->s_data,
                 rsp_ptr,
                 bytes_left,
                 0);
      if (len > 0) {
        rsp_ptr    += len;
        bytes_left -= len;
        continue;
      }
      else {
        break;
      }
    }
    /* this macro hides windows differences */
    if (CHECK_FOR_RECV_ERROR(len)) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_RECV_ERROR,
                          "data_recv_error");
      continue;
    }
    if (len == 0) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_CONNECTION_CLOSED_ERROR,
                          "data_connection_closed_early");
      continue;
    }
    len = recv(my_data->s_data,
               rsp_ptr,
               1,
               0);
    if (CHECK_FOR_RECV_ERROR(len)) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_RECV_ERROR,
                          "data_recv_error");
      continue;
    }
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_two);
    HIST_ADD(my_data->time_hist,&time_one,&time_two);
    my_data->stats.named.trans_sent++;
    my_data->recv_ring = my_data->recv_ring->next;
    my_data->send_ring = my_data->send_ring->next;
    CLOSE_SOCKET(my_data->s_data);
    my_data->s_data = -1;
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
send_tcp_crr_load(test_t *test)
{
  uint32_t          new_state;
  int               bytes_left;
  int               len;
  int               myport;
  int               rc;
  char             *rsp_ptr;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  myport    = get_port_number(my_data->locaddr);

  while (NO_STATE_CHANGE(test)) {
    /* code to make data dirty macro enabled by DIRTY */
    MAKE_DIRTY(my_data,my_data->send_ring);
    /* code to timestamp enabled by WANT_HISTOGRAM */
    /* change the port number and get a new socket */
    if (my_data->s_data == -1) {
      my_data->s_data = create_data_socket(test);
      myport++;
      if (myport >= my_data->port_max) {
        myport = my_data->port_min;
      }
      set_port_number(test, my_data->locaddr, myport);
      rc = bind(my_data->s_data,
                my_data->locaddr->ai_addr, 
                my_data->locaddr->ai_addrlen);
      if (rc < 0) {
        if (!(CHECK_FOR_EADDRINUSE || CHECK_FOR_EADDRNOTAVAIL)) {
          report_test_failure(test,
                              (char *)__func__,
                              BSDE_BIND_FAILED,
                              "data socket bind failed");
        }
        close(my_data->s_data);
        my_data->s_data = -1;
        continue;
      }
      if (connect(my_data->s_data,
                  my_data->remaddr->ai_addr,
                  my_data->remaddr->ai_addrlen) < 0) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_CONNECT_FAILED,
                            "data socket connect failed");
        close(my_data->s_data);
        my_data->s_data = -1;
        continue;
      }
    }
    /* send data for the test */
    if((len=send(my_data->s_data,
                 my_data->send_ring->buffer_ptr,
                 my_data->req_size,
                 0)) != my_data->req_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                           (char *)__func__,
                           BSDE_DATA_SEND_ERROR,
                           "data send error");
        continue;
      }
    }
    /* recv the request for the test */
    rsp_ptr    = my_data->recv_ring->buffer_ptr;
    bytes_left = my_data->rsp_size;
    while (bytes_left > 0) {
      len = recv(my_data->s_data,
                 rsp_ptr,
                 bytes_left,
                 0);
      if (len > 0) {
        rsp_ptr    += len;
        bytes_left -= len;
        continue;
      }
      else {
        break;
      }
    }
    /* this macro hides windows differences */
    if (CHECK_FOR_RECV_ERROR(len)) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_RECV_ERROR,
                          "data_recv_error");
      continue;
    }
    if (len == 0) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_CONNECTION_CLOSED_ERROR,
                          "data_connection_closed_early");
      continue;
    }
    len = recv(my_data->s_data,
               rsp_ptr,
               1,
               0);
    if (CHECK_FOR_RECV_ERROR(len)) {
      report_test_failure(test,
                          (char *)__func__,
                          BSDE_DATA_RECV_ERROR,
                          "data_recv_error");
      continue;
    }
    my_data->recv_ring = my_data->recv_ring->next;
    my_data->send_ring = my_data->send_ring->next;
    CLOSE_SOCKET(my_data->s_data);
    my_data->s_data = -1;
  }
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_MEASURE) {
    /* transitioning to measure state from loaded state
       set previous timestamp */
    gettimeofday(&(my_data->prev_time), NULL);
  }
  return(new_state);
}

int
recv_tcp_crr_clear_stats(test_t *test)
{
  return(bsd_test_clear_stats(GET_TEST_DATA(test)));
}


xmlNodePtr
recv_tcp_crr_get_stats(test_t *test)
{
  return( bsd_test_get_stats(test));
}

void
recv_tcp_crr_decode_stats(xmlNodePtr stats,test_t *test)
{
  bsd_test_decode_stats(stats,test);
}


int
send_tcp_crr_clear_stats(test_t *test)
{
  return(bsd_test_clear_stats(GET_TEST_DATA(test)));
}

xmlNodePtr
send_tcp_crr_get_stats(test_t *test)
{
  return( bsd_test_get_stats(test));
}

void
send_tcp_crr_decode_stats(xmlNodePtr stats,test_t *test)
{
  bsd_test_decode_stats(stats,test);
}


/* This routine implements the server-side of the TCP request/response */
/* test (a.k.a. rr) for the sockets interface. It receives its  */
/* parameters via the xml node contained in the test structure. */
/* results are collected by the procedure recv_tcp_crr_get_stats */

void
recv_tcp_crr(test_t *test)
{
  uint32_t state, new_state;
  bsd_test_init(test, SOCK_STREAM, IPPROTO_TCP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      recv_tcp_crr_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = recv_tcp_crr_init(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        g_usleep(1000000);
      }
      break;
    case TEST_MEASURE:
      new_state = recv_tcp_crr_meas(test);
      break;
    case TEST_LOADED:
      new_state = recv_tcp_crr_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
} /* end of recv_tcp_crr */


/* This routine implements the TCP request/response test */
/* (a.k.a. rr) for the sockets interface. It receives its */
/* parameters via the xml node contained in the test structure */
/* output to the standard output. */
/* results are collected by the procedure send_tcp_crr_get_stats */

void
send_tcp_crr(test_t *test)
{
  uint32_t state, new_state;
  bsd_test_init(test, SOCK_STREAM, IPPROTO_TCP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      send_tcp_crr_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = send_tcp_crr_init(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        g_usleep(1000000);
      }
      break;
    case TEST_MEASURE:
      new_state = send_tcp_crr_meas(test);
      break;
    case TEST_LOADED:
      new_state = send_tcp_crr_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
} /* end of send_tcp_crr */

#endif
/* end of BSD_TCP_CRR_TESTS */


#define BSD_UDP_STREAM_TESTS
#define BSD_UDP_RR_TESTS
#ifdef BSD_UDP_STREAM_TESTS

static void
recv_udp_stream_preinit(test_t *test)
{
  int               rc;         
  int               s_data;
  bsd_data_t       *my_data;
  struct sockaddr   myaddr;
  netperf_socklen_t mylen;

  my_data   = GET_TEST_DATA(test);
  mylen     = sizeof(myaddr);

  my_data->recv_ring = allocate_buffer_ring(my_data->recv_width,
                                            my_data->recv_size,
                                            my_data->recv_align,
                                            my_data->recv_offset,
                                            my_data->fill_source);
  s_data = create_data_socket(test);
  my_data->s_data = s_data;
  if (test->debug) {
    dump_addrinfo(test->where, my_data->locaddr,
                  (xmlChar *)NULL, (xmlChar *)NULL, -1);
    fprintf(test->where, 
            "%s:create_data_socket returned %d\n", 
            __func__, s_data);
    fflush(test->where);
  }
  rc = bind(s_data, my_data->locaddr->ai_addr, my_data->locaddr->ai_addrlen);
  if (test->debug) {
    fprintf(test->where, 
            "%s:bind returned %d  errno=%d\n", 
            __func__, rc, GET_ERRNO);
    fflush(test->where);
  }
  if (rc == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_BIND_FAILED,
                        "data socket bind failed");
  } else if (getsockname(s_data,&myaddr,&mylen) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_GETSOCKNAME_FAILED,
                        "getting the data socket name failed");
  } else {
    memcpy(my_data->locaddr->ai_addr,&myaddr,mylen);
    my_data->locaddr->ai_addrlen = mylen;
    set_dependent_data(test);
  }
}


static void
recv_udp_stream_idle_link(test_t *test)
{
  bsd_data_t       *my_data;
  int               len;
  int               ready;
  fd_set            readfds;
  struct timeval    timeout;


  my_data   = GET_TEST_DATA(test);

  FD_ZERO(&readfds);
  timeout.tv_sec  = 1;
  timeout.tv_usec = 0;

  FD_SET(my_data->s_data, &readfds);
  ready = select(my_data->s_data+1, &readfds, NULL, NULL, &timeout);
  while (ready > 0) {
    if ((len=recvfrom(my_data->s_data,
                      my_data->recv_ring->buffer_ptr,
                      my_data->recv_size,
                      0,0,0)) != my_data->recv_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_RECV_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_RECV_ERROR,
                            "data_recv_error");
        break;
      }
    }
    FD_SET(my_data->s_data, &readfds);
    ready = select(my_data->s_data+1, &readfds, NULL, NULL, &timeout);
  } 
}

static uint32_t
recv_udp_stream_meas(test_t *test)
{
  int               len;
  uint32_t          new_state;
  bsd_data_t       *my_data;


  my_data   = GET_TEST_DATA(test);

  while (NO_STATE_CHANGE(test)) {
    HISTOGRAM_VARS;
    /* code to make data dirty macro enabled by DIRTY */
    MAKE_DIRTY(my_data, my_data->recv_ring);
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_one);
    /* recv data for the test */
    if ((len=recvfrom(my_data->s_data,
                      my_data->recv_ring->buffer_ptr,
                      my_data->recv_size,
                      0,0,0)) != my_data->recv_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_RECV_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_RECV_ERROR,
                            "data_recv_error");
      }
    }
    my_data->stats.named.bytes_received += len;
    my_data->stats.named.recv_calls++;
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_two);
    HIST_ADD(my_data->time_hist,&time_one,&time_two);
    my_data->recv_ring = my_data->recv_ring->next;
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
recv_udp_stream_load(test_t *test)
{
  int               len;
  int               ready;
  uint32_t          new_state;
  bsd_data_t       *my_data;
  fd_set            readfds;
  struct timeval    timeout;


  my_data   = GET_TEST_DATA(test);

  FD_ZERO(&readfds);
  timeout.tv_sec  = 1;
  timeout.tv_usec = 0;

  while (NO_STATE_CHANGE(test)) {
    /* do a select call to ensure the the recvfrom will not block    */
    /* if the recvfrom blocks the test may hang on state transitions */
    FD_SET(my_data->s_data, &readfds);
    ready = select(my_data->s_data+1, &readfds, NULL, NULL, &timeout);
    if (ready > 0) {
      /* code to make data dirty macro enabled by DIRTY */
      MAKE_DIRTY(my_data, my_data->recv_ring);
      /* recv data for the test */
      if ((len=recvfrom(my_data->s_data,
                        my_data->recv_ring->buffer_ptr,
                        my_data->recv_size,
                        0,0,0)) != my_data->recv_size) {
        /* this macro hides windows differences */
        if (CHECK_FOR_RECV_ERROR(len)) {
          report_test_failure(test,
                              (char *)__func__,
                              BSDE_DATA_RECV_ERROR,
                              "data_recv_error");
        }
      }
      my_data->recv_ring = my_data->recv_ring->next;
    }
  }
  /* check for state transition */
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_MEASURE) {
    /* transitioning to measure state from loaded state
       set previous timestamp */
    gettimeofday(&(my_data->prev_time), NULL);
  }
  if (new_state == TEST_IDLE) {
    /* a request to transition to the idle state */
    recv_udp_stream_idle_link(test);
  }
  return(new_state);
}


static void
send_udp_stream_preinit(test_t *test)
{
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  if (my_data->s_data == 0) {
    my_data->send_ring = allocate_buffer_ring(my_data->send_width,
                                              my_data->send_size,
                                              my_data->send_align,
                                              my_data->send_offset,
                                              my_data->fill_source);
    get_dependency_data(test, SOCK_DGRAM, IPPROTO_UDP);
    my_data->s_data = create_data_socket(test);
  } else {
    fprintf(test->where,"entered %s more than once\n",__func__);
    fflush(test->where);
  }
}


static uint32_t
send_udp_stream_init(test_t *test)
{
  int               rc;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  if (test->debug) {
    fprintf(test->where,"%s: in INIT state making bind call\n",__func__);
    fflush(test->where);
  }
  rc = bind(my_data->s_data,
            my_data->locaddr->ai_addr,
            my_data->locaddr->ai_addrlen);
  if (test->debug) {
    fprintf(test->where, 
            "%s:bind returned %d  errno=%d\n", 
            __func__, rc, GET_ERRNO);
    fflush(test->where);
  }
  if (rc == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_BIND_FAILED,
                        "data socket bind failed");
  }
  return(TEST_IDLE);
}


static uint32_t
send_udp_stream_meas(test_t *test)
{
  int               len;
  uint32_t          new_state;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  while (NO_STATE_CHANGE(test)) {
    HISTOGRAM_VARS;
    /* code to make data dirty macro enabled by DIRTY */
    MAKE_DIRTY(my_data,my_data->send_ring);
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_one);
    /* send data for the test */
    if((len=sendto(my_data->s_data,
                   my_data->send_ring->buffer_ptr,
                   my_data->send_size,
                   0,
                   my_data->remaddr->ai_addr,
                   my_data->remaddr->ai_addrlen)) != my_data->send_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data send error");
      }
    }
    my_data->stats.named.bytes_sent += len;
    my_data->stats.named.send_calls++;
    my_data->send_ring = my_data->send_ring->next;
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_two);
    HIST_ADD(my_data->time_hist,&time_one,&time_two);
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
send_udp_stream_load(test_t *test)
{
  int               len;
  uint32_t          new_state;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  while (NO_STATE_CHANGE(test)) {
    /* code to make data dirty macro enabled by DIRTY */
    MAKE_DIRTY(my_data,my_data->send_ring);
    /* send data for the test */
    if((len=sendto(my_data->s_data,
                   my_data->send_ring->buffer_ptr,
                   my_data->send_size,
                   0,
                   my_data->remaddr->ai_addr,
                   my_data->remaddr->ai_addrlen)) != my_data->send_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data send_error");
      }
    }
    my_data->send_ring = my_data->send_ring->next;
  }
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_MEASURE) {
    /* transitioning to measure state from loaded state
       set previous timestamp */
    gettimeofday(&(my_data->prev_time), NULL);
  }
  return(new_state);
}

int
recv_udp_stream_clear_stats(test_t *test)
{
  return(bsd_test_clear_stats(GET_TEST_DATA(test)));
}


xmlNodePtr
recv_udp_stream_get_stats(test_t *test)
{
  return( bsd_test_get_stats(test));
}

void
recv_udp_stream_decode_stats(xmlNodePtr stats,test_t *test)
{
  bsd_test_decode_stats(stats,test);
}


int
send_udp_stream_clear_stats(test_t *test)
{
  return(bsd_test_clear_stats(GET_TEST_DATA(test)));
}

xmlNodePtr
send_udp_stream_get_stats(test_t *test)
{
  return( bsd_test_get_stats(test));
}

void
send_udp_stream_decode_stats(xmlNodePtr stats,test_t *test)
{
  bsd_test_decode_stats(stats,test);
}


/* This routine implements the server-side of the UDP unidirectional data */
/* transfer test (a.k.a. stream) for the sockets interface. It receives */
/* its parameters via the xml node contained in the test structure. */
/* results are collected by the procedure recv_udp_stream_get_stats */

void
recv_udp_stream(test_t *test)
{
  uint32_t state, new_state;
  bsd_test_init(test, SOCK_DGRAM, IPPROTO_UDP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      recv_udp_stream_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = TEST_IDLE;
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        sleep(1);
      }
      break;
    case TEST_MEASURE:
      new_state = recv_udp_stream_meas(test);
      break;
    case TEST_LOADED:
      new_state = recv_udp_stream_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
} /* end of recv_udp_stream */


/* This routine implements the UDP unidirectional data transfer test */
/* (a.k.a. stream) for the sockets interface. It receives its */
/* parameters via the xml node contained in the test structure */
/* output to the standard output. */
/* results are collected by the procedure send_udp_stream_get_stats */

void
send_udp_stream(test_t *test)
{
  uint32_t state, new_state;
  bsd_test_init(test, SOCK_DGRAM, IPPROTO_UDP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      send_udp_stream_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = send_udp_stream_init(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        sleep(1);
      }
      break;
    case TEST_MEASURE:
      new_state = send_udp_stream_meas(test);
      break;
    case TEST_LOADED:
      new_state = send_udp_stream_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
}  /* end of send_udp_stream */


#endif
/* end of BSD_UDP_STREAM_TESTS */
#ifdef BSD_UDP_RR_TESTS

static void
recv_udp_rr_preinit(test_t *test)
{
  int               rc;
  int               s_data;
  bsd_data_t       *my_data;
  struct sockaddr   myaddr;
  netperf_socklen_t mylen;

  my_data   = GET_TEST_DATA(test);
  mylen     = sizeof(myaddr);

  my_data->recv_ring = allocate_buffer_ring(my_data->recv_width,
                                            my_data->req_size,
                                            my_data->recv_align,
                                            my_data->recv_offset,
                                            my_data->fill_source);
  my_data->send_ring = allocate_buffer_ring(my_data->send_width,
                                            my_data->rsp_size,
                                            my_data->send_align,
                                            my_data->send_offset,
                                            my_data->fill_source);
  s_data = create_data_socket(test);
  my_data->s_data = s_data;
  if (test->debug) {
    dump_addrinfo(test->where, my_data->locaddr,
                  (xmlChar *)NULL, (xmlChar *)NULL, -1);
    fprintf(test->where, 
            "%s:create_data_socket returned %d\n", 
            __func__, s_data);
    fflush(test->where);
  }
  rc = bind(s_data, my_data->locaddr->ai_addr, my_data->locaddr->ai_addrlen);
  if (test->debug) {
    fprintf(test->where, 
            "%s:bind returned %d  errno=%d\n", 
            __func__, rc, GET_ERRNO);
    fflush(test->where);
  }
  if (rc == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_BIND_FAILED,
                        "data socket bind failed");
  } else if (getsockname(s_data,&myaddr,&mylen) == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_GETSOCKNAME_FAILED,
                        "getting the data socket name failed");
  } else {
    memcpy(my_data->locaddr->ai_addr,&myaddr,mylen);
    my_data->locaddr->ai_addrlen = mylen;
    set_dependent_data(test);
  }
}


static void
recv_udp_rr_idle_link(test_t *test)
{
  int               len;
  bsd_data_t       *my_data;
  int               ready;
  fd_set            readfds;
  struct timeval    timeout;

  my_data   = GET_TEST_DATA(test);

  FD_ZERO(&readfds);
  timeout.tv_sec  = 1;
  timeout.tv_usec = 0;

  FD_SET(my_data->s_data, &readfds);
  ready = select(my_data->s_data+1, &readfds, NULL, NULL, &timeout);
  while (ready > 0) {
    if ((len=recvfrom(my_data->s_data,
                      my_data->recv_ring->buffer_ptr,
                      my_data->req_size,
                      0,0,0)) != my_data->req_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_RECV_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_RECV_ERROR,
                            "data_recv_error");
        break;
      }
    }
    FD_SET(my_data->s_data, &readfds);
    ready = select(my_data->s_data+1, &readfds, NULL, NULL, &timeout);
  }
}

static uint32_t
recv_udp_rr_meas(test_t *test)
{
  int               len;
  uint32_t          new_state;
  bsd_data_t       *my_data;
  struct sockaddr   peeraddr;
  netperf_socklen_t peerlen;


  my_data   = GET_TEST_DATA(test);

  while(NO_STATE_CHANGE(test)) {
    HISTOGRAM_VARS;
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_one);
    /* recv the request for the test */
    peerlen = sizeof(struct sockaddr);
    if ((len=recvfrom(my_data->s_data,
                      my_data->recv_ring->buffer_ptr,
                      my_data->req_size,
                      0,
                      &peeraddr,
                      &peerlen)) != my_data->req_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_RECV_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_RECV_ERROR,
                            "data_recv_error");
        break;
      }
    }
    my_data->stats.named.trans_received++;
    my_data->stats.named.bytes_received += len;
    if ((len=sendto(my_data->s_data,
                    my_data->send_ring->buffer_ptr,
                    my_data->rsp_size,
                    0,
                    &peeraddr,
                    peerlen)) != my_data->rsp_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data_send_error");
        break;
      }
    }
    my_data->stats.named.bytes_sent += len;
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_two);
    HIST_ADD(my_data->time_hist,&time_one,&time_two);
    my_data->recv_ring = my_data->recv_ring->next;
    my_data->send_ring = my_data->send_ring->next;
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
recv_udp_rr_load(test_t *test)
{
  int               len;
  int               ready;
  uint32_t          new_state;
  bsd_data_t       *my_data;
  struct sockaddr   peeraddr;
  netperf_socklen_t peerlen;
  fd_set            readfds;
  struct timeval    timeout;


  my_data   = GET_TEST_DATA(test);

  FD_ZERO(&readfds);
  timeout.tv_sec  = 1;
  timeout.tv_usec = 0;

  while (NO_STATE_CHANGE(test)) {
    /* do a select call to ensure the the recvfrom will not block    */
    /* if the recvfrom blocks the test may hang on state transitions */
    FD_SET(my_data->s_data, &readfds);
    ready = select(my_data->s_data+1, &readfds, NULL, NULL, &timeout);
    if (ready > 0) {
      peerlen = sizeof(struct sockaddr);
      if ((len=recvfrom(my_data->s_data,
                        my_data->recv_ring->buffer_ptr,
                        my_data->req_size,
                        0,
                        &peeraddr,
                        &peerlen)) != my_data->req_size) {
        /* this macro hides windows differences */
        if (CHECK_FOR_RECV_ERROR(len)) {
          report_test_failure(test,
                              (char *)__func__,
                              BSDE_DATA_RECV_ERROR,
                              "data_recv_error");
          break;
        }
      }
      /* send the response to the test */
      if ((len=sendto(my_data->s_data,
                      my_data->send_ring->buffer_ptr,
                      my_data->rsp_size,
                      0,
                      &peeraddr,
                      peerlen)) != my_data->rsp_size) {
        /* this macro hides windows differences */
        if (CHECK_FOR_SEND_ERROR(len)) {
          report_test_failure(test,
                              (char *)__func__,
                              BSDE_DATA_SEND_ERROR,
                              "data_send_error");
          break;
        }
      }
      my_data->recv_ring = my_data->recv_ring->next;
      my_data->send_ring = my_data->send_ring->next;
    }
  }
  /* check for state transition */
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_MEASURE) {
    /* transitioning to measure state from loaded state
       set previous timestamp */
    gettimeofday(&(my_data->prev_time), NULL);
  }
  if (new_state == TEST_IDLE) {
    recv_udp_rr_idle_link(test);
  }
  return(new_state);
}

static void
send_udp_rr_preinit(test_t *test)
{
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  my_data->recv_ring = allocate_buffer_ring(my_data->recv_width,
                                            my_data->rsp_size,
                                            my_data->recv_align,
                                            my_data->recv_offset,
                                            my_data->fill_source);
  my_data->send_ring = allocate_buffer_ring(my_data->send_width,
                                            my_data->req_size,
                                            my_data->send_align,
                                            my_data->send_offset,
                                            my_data->fill_source);

  get_dependency_data(test, SOCK_DGRAM, IPPROTO_UDP);
  my_data->s_data = create_data_socket(test);
}

static uint32_t
send_udp_rr_init(test_t *test)
{
  int               rc;
  bsd_data_t       *my_data;

  my_data   = GET_TEST_DATA(test);

  if (test->debug) {
    fprintf(test->where,"%s: in INIT state making bind call\n",__func__);
    fflush(test->where);
  }
  rc = bind(my_data->s_data,
            my_data->locaddr->ai_addr,
            my_data->locaddr->ai_addrlen);
  if (test->debug) {
    fprintf(test->where, 
            "%s:bind returned %d  errno=%d\n", 
            __func__, rc, GET_ERRNO);
    fflush(test->where);
  }
  if (rc == -1) {
    report_test_failure(test,
                        (char *)__func__,
                        BSDE_CONNECT_FAILED,
                        "data socket bind failed");
  }
  /* should we add a connect here ?? */
  return(TEST_IDLE);
}

static void
send_udp_rr_idle_link(test_t *test)
{
  int               len;
  bsd_data_t       *my_data;
  int               ready;
  fd_set            readfds;
  struct timeval    timeout;


  my_data   = GET_TEST_DATA(test);

  FD_ZERO(&readfds);
  timeout.tv_sec  = 1;
  timeout.tv_usec = 0;

  FD_SET(my_data->s_data, &readfds);
  ready = select(my_data->s_data+1, &readfds, NULL, NULL, &timeout);
  while (ready > 0) {
    if ((len=recvfrom(my_data->s_data,
                      my_data->recv_ring->buffer_ptr,
                      my_data->rsp_size,
                      0,0,0)) != my_data->rsp_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_RECV_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_RECV_ERROR,
                            "data_recv_error");
        break;
      }
    }
    FD_SET(my_data->s_data, &readfds);
    ready = select(my_data->s_data+1, &readfds, NULL, NULL, &timeout);
  }
}

static uint32_t
send_udp_rr_meas(test_t *test)
{
  uint32_t          new_state;
  int               len;
  bsd_data_t       *my_data;
  char             *buffer_ptr;


  NETPERF_DEBUG_ENTRY(test->debug, test->where);
  my_data   = GET_TEST_DATA(test);

  while (NO_STATE_CHANGE(test)) {
    HISTOGRAM_VARS;
    /* code to make data dirty macro enabled by DIRTY */
    MAKE_DIRTY(my_data,my_data->send_ring);
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_one);
    /* send data for the test */
    buffer_ptr = my_data->send_ring->buffer_ptr;
    UDP_CHECK_FOR_RETRANS;
    if((len=sendto(my_data->s_data,
                   buffer_ptr,
                   my_data->req_size,
                   0,
                   my_data->remaddr->ai_addr,
                   my_data->remaddr->ai_addrlen)) != my_data->req_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data send error");
        break;
      }
    }
    my_data->stats.named.trans_sent++;
    my_data->stats.named.bytes_sent += len;
    UDP_CHECK_BURST_SEND;
    /* recv the request for the test */
    buffer_ptr = my_data->recv_ring->buffer_ptr;
    if ((len=recvfrom(my_data->s_data,
                      buffer_ptr,
                      my_data->rsp_size,
                      0,0,0)) != my_data->rsp_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_RECV_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_RECV_ERROR,
                            "data_recv_error");
        break;
      }
    }
    my_data->stats.named.bytes_received += len;
    UDP_CHECK_BURST_RECV;
    /* code to timestamp enabled by WANT_HISTOGRAM */
    HIST_TIMESTAMP(&time_two);
    HIST_ADD(my_data->time_hist,&time_one,&time_two);
    my_data->send_ring = my_data->send_ring->next;
    my_data->recv_ring = my_data->recv_ring->next;
  }
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_LOADED) {
    /* transitioning to loaded state from measure state
       set current timestamp and update elapsed time */
    gettimeofday(&(my_data->curr_time), NULL);
    update_elapsed_time(my_data);
  }
  NETPERF_DEBUG_EXIT(test->debug, test->where);
  return(new_state);
}

static uint32_t
send_udp_rr_load(test_t *test)
{
  uint32_t          new_state;
  int               len;
  bsd_data_t       *my_data;
  char             *buffer_ptr;


  NETPERF_DEBUG_ENTRY(test->debug, test->where);
  my_data   = GET_TEST_DATA(test);

  while (NO_STATE_CHANGE(test)) {
    /* code to make data dirty macro enabled by DIRTY */
    MAKE_DIRTY(my_data,my_data->send_ring);
    /* send data for the test */
    buffer_ptr  = my_data->send_ring->buffer_ptr;
    UDP_CHECK_FOR_RETRANS;
    if((len=sendto(my_data->s_data,
                 buffer_ptr,
                 my_data->req_size,
                 0,
                 my_data->remaddr->ai_addr,
                 my_data->remaddr->ai_addrlen)) != my_data->req_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_SEND_ERROR,
                            "data send error");
        break;
      }
    }
    UDP_CHECK_BURST_SEND;
    buffer_ptr  = my_data->recv_ring->buffer_ptr;
    /* recv the request for the test */
    if ((len=recvfrom(my_data->s_data,
                      buffer_ptr,
                      my_data->rsp_size,
                      0,0,0)) != my_data->rsp_size) {
      /* this macro hides windows differences */
      if (CHECK_FOR_RECV_ERROR(len)) {
        report_test_failure(test,
                            (char *)__func__,
                            BSDE_DATA_RECV_ERROR,
                            "data_recv_error");
        break;
      }
    }
    UDP_CHECK_BURST_RECV;
    my_data->send_ring = my_data->send_ring->next;
    my_data->recv_ring = my_data->recv_ring->next;
  }
  new_state = CHECK_REQ_STATE;
  if (new_state == TEST_MEASURE) {
    /* transitioning to measure state from loaded state
       set previous timestamp */
    gettimeofday(&(my_data->prev_time), NULL);
  }
  if (new_state == TEST_IDLE) {
    send_udp_rr_idle_link(test);
  }
  NETPERF_DEBUG_EXIT(test->debug, test->where);
  return(new_state);
}

int
recv_udp_rr_clear_stats(test_t *test)
{
  return(bsd_test_clear_stats(GET_TEST_DATA(test)));
}


xmlNodePtr
recv_udp_rr_get_stats(test_t *test)
{
  return( bsd_test_get_stats(test));
}

void
recv_udp_rr_decode_stats(xmlNodePtr stats,test_t *test)
{
  bsd_test_decode_stats(stats,test);
}


int
send_udp_rr_clear_stats(test_t *test)
{
  return(bsd_test_clear_stats(GET_TEST_DATA(test)));
}

xmlNodePtr
send_udp_rr_get_stats(test_t *test)
{
  return( bsd_test_get_stats(test));
}

void
send_udp_rr_decode_stats(xmlNodePtr stats,test_t *test)
{
  bsd_test_decode_stats(stats,test);
}


/* This routine implements the server-side of the TCP request/response */
/* test (a.k.a. rr) for the sockets interface. It receives its  */
/* parameters via the xml node contained in the test structure. */
/* results are collected by the procedure recv_udp_rr_get_stats */

void
recv_udp_rr(test_t *test)
{
  uint32_t state, new_state;
  bsd_test_init(test, SOCK_DGRAM, IPPROTO_UDP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      recv_udp_rr_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = TEST_IDLE;
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        sleep(1);
      }
      break;
    case TEST_MEASURE:
      new_state = recv_udp_rr_meas(test);
      break;
    case TEST_LOADED:
      new_state = recv_udp_rr_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
} /* end of recv_udp_rr */


/* This routine implements the TCP request/responce test */
/* (a.k.a. rr) for the sockets interface. It receives its */
/* parameters via the xml node contained in the test structure */
/* output to the standard output. */
/* results are collected by the procedure send_udp_rr_get_stats */

void
send_udp_rr(test_t *test)
{
  uint32_t state, new_state;
  bsd_test_init(test, SOCK_DGRAM, IPPROTO_UDP);
  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      send_udp_rr_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = send_udp_rr_init(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        sleep(1);
      }
      break;
    case TEST_MEASURE:
      new_state = send_udp_rr_meas(test);
      break;
    case TEST_LOADED:
      new_state = send_udp_rr_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
} /* end of send_udp_rr */

#endif
/* end of BSD_UDP_STREAM_TESTS */



/*  This implementation of report_bsd_test_results will generate strange
    results if transaction count and throughput tests are included in the
    same test set. The first test in the set sets the headers and algorithm
    for computing service demand */

void
bsd_test_results_init(tset_t *test_set, char *report_flags, char *output)
{
  bsd_results_t *rd;
  FILE          *outfd;
  int            max_count;
  size_t         malloc_size;

  rd        = test_set->report_data;
  max_count = test_set->confidence.max_count;
  
  if (output) {
    if (test_set->debug) {
      fprintf(test_set->where,
              "%s: report going to file %s\n",
	      __func__,
              output);
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
  malloc_size = sizeof(bsd_results_t) + 7 * max_count * sizeof(double);
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
    /* we should initialize result_confidence here? */
    rd->result_confidence = 0.0;
    rd->result_minimum = DBL_MAX;
    rd->result_maximum = DBL_MIN;
    rd->outfd          = outfd;
    rd->sd_denominator = 0.0;
    if (NULL != report_flags) {
      if (!strcmp(report_flags,"PRINT_RUN")) {
	rd->print_run     = 1;
      }
      if (!strcmp(report_flags,"PRINT_TESTS")) {
	rd->print_test    = 1;
      }
      if (!strcmp(report_flags,"PRINT_ALL")) {
	rd->print_run     = 1;
	rd->print_test    = 1;
	rd->print_per_cpu = 1;
      }
      /* PN: Should do CSV (comma separated values) reporting? */
      if (!strcmp(report_flags,"PRINT_CSV")) {
        rd->print_csv    = 1;
        /* Timestamp for all tests in this set, helps to group
         * related tests in the CSV file
         */
        time(&rd->timestamp);
      }
      memset(&(rd->csv_files), 0, sizeof(csv_files_t));
    }
    if (test_set->debug) {
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
    exit(-1);
  }
}

static void
process_test_stats(tset_t *test_set, xmlNodePtr stats, xmlChar *tid)
{
  int            i;
  int            count;
  int            index;
  FILE          *outfd;
  FILE		*csvfd;
  bsd_results_t *rd;

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
  if (test_set->debug) {
    fprintf(test_set->where,"\tprocessing test_stats\n");
    fflush(test_set->where);
  }
  for (i=0; i<MAX_TEST_CNTRS; i++) {
    char *value_str =
       (char *)xmlGetProp(stats, (const xmlChar *)cntr_name[i]);
    if (value_str) {
      test_cntr[i] = strtod(value_str,NULL);
      if (test_cntr[i] == 0.0) {
        uint64_t x;
	/* MUST use the PRIx64 macro to get the proper format under
           Windows or we get garbage.  raj 2006-04-12 */
        sscanf(value_str,"%"PRIx64,&x);
        test_cntr[i] = (double)x;
      }
    }
    else {
      test_cntr[i] = 0.0;
    }
    if (test_set->debug) {
      unsigned char *string = NULL;
      string = xmlGetProp(stats, (const xmlChar *)cntr_name[i]);
      fprintf(test_set->where,"\t%12s test_cntr[%2d] = %10g\t'%s'\n",
              cntr_name[i], i, test_cntr[i],
	      string ? (char *)string : "n/a");
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
            recv_trans_rate, test_cntr[TST_R_TRANS]);
    fflush(test_set->where);
  }
  if (rd->sd_denominator == 0.0) {
    if (xmit_trans_rate > 0.0 || recv_trans_rate > 0.0) {
      rd->sd_denominator = 1.0;
    } else {
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
    if (rd->sd_denominator != 1.0) {
      fprintf(outfd,"%7.2f ",result);                 /* 19,8 */
      fprintf(outfd,"%7.2f ",xmit_rate);              /* 27,8 */
      fprintf(outfd,"%7.2f ",recv_rate);              /* 35,8 */
    }
    else {
      fprintf(outfd,"%10.2f ",result);                /* 19,11 */
    }
    fprintf(outfd,"\n");
    fflush(outfd);
  }
  /* end of printing bsd per test instance results */

  if(rd->print_csv) {
    /* CSV output */
    csvfd = rd->csv_files.test_stats;
    fprintf(csvfd, "%ld,%s,%s", rd->timestamp, (char *)test_set->id,
        (char *)tid);
    fprintf(csvfd, ",%10.2f,%10.2f,%10.2f,%10.2f\n",
        xmit_rate, xmit_trans_rate, recv_rate, recv_trans_rate);
    fflush(csvfd);
  }
}

/* PN: 06/27/2007
 * Function prints sys_stat counters as comma separated values (CSV)
 */
static void
print_csv_sys_stats(xmlNodePtr stats, double elapsed_sec, char *cpuid,
  time_t timestamp, const char *test_set_id, const char *test_id, FILE *outfd)
{

#define NUM_CPU_CNTRS           9
#define CPU_CNTR_CALIBRATE      0
#define CPU_CNTR_USER           1
#define CPU_CNTR_NICE           2
#define CPU_CNTR_KERN           3
#define CPU_CNTR_IDLE           4
#define CPU_CNTR_IOWAIT         5
#define CPU_CNTR_INTR           6
#define CPU_CNTR_SIRQ           7
#define CPU_CNTR_INTRATE        8

  const char *cpu_cntr_names[] = {
    "calibration",
    "user_count",
    "nice_count",
    "sys_count",
    "idle_count",
    "iowait_count",
    "int_count",
    "softirq_count",
    "total_interrupts"
  };

  double        calibration;
  double        metric;
  char          *value_str;
  uint64_t      x;
  int           i;

  value_str = (char *)xmlGetProp(stats, (const xmlChar *)"calibration");
  if (value_str) {
    calibration = strtod(value_str,NULL);
    if (calibration == 0.0) {      
        sscanf(value_str,"%"PRIx64,&x);
        calibration = (double)x;
    }
  }
  else {
    calibration = 0.0;
  }

  /* CSV: Print this CPU's information */
  fprintf(outfd, "%ld,%s,%s", timestamp, test_set_id, test_id);

  fprintf(outfd, ",%s", cpuid);
  for (i=1; i<NUM_CPU_CNTRS; i++) {
    value_str = (char *)xmlGetProp(stats, (const xmlChar *)cpu_cntr_names[i]);
    if (value_str) {
      metric = strtod(value_str,NULL);
      if (metric == 0.0) {        
        sscanf(value_str,"%"PRIx64,&x);
        metric = (double)x;
      }
    }
    else {
      metric = 0.0;
    }
    if (i == CPU_CNTR_INTRATE )
      fprintf(outfd, ",%10.2f", metric / elapsed_sec);
    else
      fprintf(outfd, ",%6.2f", metric * 100.00 / calibration);
  }
  fprintf(outfd, "\n");
  fflush(outfd);
}

static double
process_sys_stats(tset_t *test_set, xmlNodePtr stats, xmlChar *tid)
{
  int            i;
  int            count;
  int            index;
  FILE          *outfd;
  bsd_results_t *rd;
  char          *value_str;
  xmlNodePtr     cpu;
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
    value_str =
       (char *)xmlGetProp(stats, (const xmlChar *)sys_cntr_name[i]);
    if (value_str) {
      sys_cntr[i] = strtod(value_str,NULL);
      if (sys_cntr[i] == 0.0) {
        uint64_t x;
	/* MUST use PRIx64 to get proper results under Windows */
        sscanf(value_str,"%"PRIx64,&x);
        sys_cntr[i] = (double)x;
      }
    }
    else {
      sys_cntr[i] = 0.0;
    }
    if (test_set->debug) {
      unsigned char *string = NULL;
      string = xmlGetProp(stats, (const xmlChar *)sys_cntr_name[i]);
      fprintf(test_set->where,
	      "\t%12s sys_stats[%d] = %10g '%s'\n",
              sys_cntr_name[i],
	      i,
	      sys_cntr[i],
	      string ? (char *) string : "n/a");
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

        /* PN: 06/27/2007
         * Other cpu stats we are interested in
         */
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"nice_count");
        fprintf(outfd,"%s ", value_str);
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"iowait_count");
        fprintf(outfd,"%s ", value_str);
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"softirq_count");
        fprintf(outfd,"%s ", value_str);
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"other_count");
        fprintf(outfd,"%s\n", value_str);

        /* PN: Total interrupts serviced by this cpu */
        value_str = (char *)xmlGetProp(cpu, (const xmlChar *)"total_interrupts");
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
    if (rd->sd_denominator != 1.0) {
      fprintf(outfd,"%24s","");                       /* 19,24*/
    }
    else {
      fprintf(outfd,"%11s","");                       /* 19,11*/
    }
    fprintf(outfd,"%7.1e ",calibration);              /* 43,8 */
    fprintf(outfd,"%6.3f ",local_idle*100.0);         /* 51,7 */
    fprintf(outfd,"%6.3f ",local_busy*100.0);         /* 58,7 */
    fprintf(outfd,"\n");                              /* 79,1 */
    fflush(outfd);
  }

  if (rd->print_csv) {
     /* CSV output */
     print_csv_sys_stats(stats, elapsed_seconds, "ALL",
                     rd->timestamp, (char *)test_set->id,
                     (char *)tid, rd->csv_files.sys_stats);
     /* Print per cpu stats */
     cpu = stats->xmlChildrenNode;
     while (cpu != NULL) {
       print_csv_sys_stats(cpu, elapsed_seconds,
                (char *)xmlGetProp(cpu, (const xmlChar *)"cpu_id"),
                rd->timestamp, (char *)test_set->id, (char *)tid,
                rd->csv_files.sys_stats);
       cpu = cpu->next;
     }
   }
  /* end of printing sys stats instance results */
  return(local_cpus);
}

/* PN: 07/05/2007.
 * Network device statistics processing
 */
static void
process_netdev_stats(tset_t *test_set, xmlNodePtr stats, xmlChar *tid)
{
  int            i;
  bsd_results_t *rd;
  xmlNodePtr    interface;
  FILE          *csvfd;
  FILE          *outfd;
  unsigned char* value_str;
  int            count;

#define NUM_NETDEV_CNTRS        8
#define NETDEV_DEVNAME          0
#define NETDEV_RCVPKTS          1
#define NETDEV_RCVERRS          2
#define NETDEV_RCVDRPS          3
#define NETDEV_TRXPKTS          4
#define NETDEV_TRXERRS          5
#define NETDEV_TRXDRPS          6
#define NETDEV_TRXCOLLS         7

  /* Netdev stats counter names.
   * These names should be identical to definitions in DTD.
   */
  const char *netdev_cntr_name[] = {
    "interface_name",
    "rcv_packets",
    "rcv_errors",
    "rcv_drops",
    "trx_packets",
    "trx_errors",
    "trx_drops",
    "trx_collisions"
  };

  rd     = test_set->report_data;
  count  = test_set->confidence.count;
  outfd  = rd->outfd;

  NETPERF_DEBUG_ENTRY(test_set->debug,test_set->where);

  if (test_set->debug) {
    fprintf(test_set->where,"\tprocessing netdev_stats\n");
    fflush(test_set->where);
  }

  if (rd->print_test) {
    /* Display per test results */
    fprintf(outfd,"%3d  ", count);
    fprintf(outfd,"%-6s ",  tid);
    interface = stats->xmlChildrenNode;
    while (interface != NULL) {
      if (!xmlStrcmp(interface->name, (const xmlChar *)"per_interface_stats")) {
        for ( i = 0 ; i < NUM_NETDEV_CNTRS; i++) {
          value_str = xmlGetProp(interface, (const xmlChar *)netdev_cntr_name[i]);
          fprintf(outfd, "%s  ", value_str ? (char *)value_str : "Nan  ");
        }
      }

      interface = interface->next;
      if (interface)
        fprintf(outfd,"\n            ");
      else
        fprintf(outfd,"\n");
      fflush(outfd);
    }
  }

  if (rd->print_csv) {
    csvfd = rd->csv_files.netdev_stats;

    /* CSV output */
    interface = stats->xmlChildrenNode;
    while (interface != NULL) {
      if (!xmlStrcmp(interface->name, (const xmlChar *)"per_interface_stats")) {
        fprintf(csvfd, "%ld,%s,%s", rd->timestamp, (char *)test_set->id,
                                        (char *)tid);
        for ( i = 0 ; i < NUM_NETDEV_CNTRS; i++) {
          value_str = xmlGetProp(interface, (const xmlChar *)netdev_cntr_name[i]);
          if (test_set->debug) {
            fprintf(test_set->where,
              "\t%15s = %6s\n", netdev_cntr_name[i],
                                value_str ? (char *) value_str : "Nan");
          }
          fprintf(csvfd, ",%s", value_str ? (char *)value_str : "Nan");
        }
        fprintf(csvfd,"\n");
        fflush(csvfd);
      }
      interface = interface->next;
    }
  }

  NETPERF_DEBUG_EXIT(test_set->debug,test_set->where);
}

/* PN: 06/29/2007.
 * TCP statistics processing
 */
static void
process_tcp_stats(tset_t *test_set, xmlNodePtr stats, xmlChar *tid)
{
  int            i;
  bsd_results_t *rd;
  FILE          *csvfd;
  FILE          *outfd;
  unsigned char* value_str;
  int            count;

#define NUM_TCP_CNTRS           4
#define TCP_LOSS                0
#define TCP_LOST_RTXMTS         1
#define TCP_FAST_RTXMTS         2
#define TCP_TIMEOUTS            3

  /* Tcp stats counter names.
   * These names should be identical to definitions in DTD.
   */
  const char *tcp_cntr_name[] = {
    "tcp_loss",
    "tcp_lost_rtxmts",
    "tcp_fast_rtxmts",
    "tcp_timeouts"
  };

  rd     = test_set->report_data;
  count  = test_set->confidence.count;
  outfd  = rd->outfd;

  NETPERF_DEBUG_ENTRY(test_set->debug,test_set->where);

  if (test_set->debug) {
    fprintf(test_set->where,"\tprocessing tcp_stats\n");
    fflush(test_set->where);
  }

  if (rd->print_test) {
    /* Display per test results */
    fprintf(outfd,"%3d  ", count);
    fprintf(outfd,"%-6s ",  tid);
    for (i=0; i<NUM_TCP_CNTRS; i++) {
      value_str = xmlGetProp(stats, (const xmlChar *)tcp_cntr_name[i]);
      fprintf(outfd, "%s  ", value_str ? (char *) value_str : "Nan  ");
    }
    fprintf(outfd,"\n");
    fflush(outfd);
  }

 if (rd->print_csv) {
    csvfd = rd->csv_files.tcp_stats;

    /* CSV output */
    fprintf(csvfd, "%ld,%s,%s", rd->timestamp, (char *)test_set->id, (char *)tid);

    for (i=0; i<NUM_TCP_CNTRS; i++) {
      value_str = xmlGetProp(stats, (const xmlChar *)tcp_cntr_name[i]);
      if (test_set->debug) {
        fprintf(test_set->where,
              "\t%15s = %6s\n", tcp_cntr_name[i],
                                  value_str ? (char *) value_str : "Nan");
      }
      fprintf(csvfd, ",%6s", value_str ? (char *) value_str : "Nan");
    }
    fprintf(csvfd,"\n");
    fflush(csvfd);
  }

  NETPERF_DEBUG_EXIT(test_set->debug,test_set->where);
}

/* PN: Necessary preprocessing for CSV reports */
static void
open_csv_file(FILE **fp, const char* test_type, test_t *test)
{
    char fname[50];
    strcpy(fname, test_type);
    strcat(fname, ".csv");

    if (!(*fp)) {
        *fp = fopen(fname,"a");
        if (!(*fp)) {
          report_test_failure(test, (char *)__func__,
                BSDE_CSV_FILE_OPEN_ERROR, "CSV file open failed");
          exit (-1);
        }
    }

    if (strcmp ((char *)test_type, "test_stats") == 0) {
        fprintf(*fp, "Timestamp,TestSetId,TestId,DataXmitRate,TransXmitRate,DataRcvRate,TransRcvRate\n");
    }

    if (strcmp ((char *)test_type, "sys_stats") == 0) {
        fprintf(*fp, "Timestamp,TestSetId,TestId,Cpuid,User,Nice,Sys,Idle,Iowait,Hirq,Sirq,IntrRate\n");
    }

    if (strcmp ((char *)test_type, "tcp_stats") == 0) {
        fprintf(*fp, "Timestamp,TestSetId,TestId,Loss,LostRtx,FastRtx,Timeouts\n");
      }

    if (strcmp ((char *)test_type, "netdev_stats") == 0) {
        fprintf(*fp,
          "Timestamp,TestSetId,TestId,Interface,RcvPkts,RcvErrs,RcvDrops,TrxPkts,TrxErrs,TrxDrops,Collisions\n");
    }
}

/* PN: Close CSV output files */
static void
close_csv_files(bsd_results_t *rd)
{

  if (rd->csv_files.test_stats)
    fclose(rd->csv_files.test_stats);
  if (rd->csv_files.sys_stats)
    fclose(rd->csv_files.sys_stats);
  if (rd->csv_files.tcp_stats)
    fclose(rd->csv_files.tcp_stats);
  if (rd->csv_files.netdev_stats)
    fclose(rd->csv_files.netdev_stats);

  memset(&(rd->csv_files), 0, sizeof(csv_files_t));

}


static void
process_stats_for_run(tset_t *test_set)
{
  bsd_results_t *rd;
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

  /* PN: Timestamp helps to identify test runs across CSV files.
   */
  if(rd->print_csv) {
    time(&(rd->timestamp));
  }

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
    while(stats != NULL) {
      /* process all the statistics records for this test */
      if (test_set->debug) {
        fprintf(test_set->where,"\tfound some statistics");
        fflush(test_set->where);
      }
      if(!xmlStrcmp(stats->name,(const xmlChar *)"sys_stats")) {
        /* process system statistics */
	if (rd->print_csv) open_csv_file(&(rd->csv_files.sys_stats), "sys_stats", test);
        num_of_cpus = process_sys_stats(test_set, stats, test->id);
        stats_for_test++;
      }
      /* PN: 06/29/2007. Process Tcp statistics */
      if(!xmlStrcmp(stats->name,(const xmlChar *)"tcp_stats")) {
	if (rd->print_csv) open_csv_file(&(rd->csv_files.tcp_stats), "tcp_stats", test);
        process_tcp_stats(test_set, stats, test->id);
        stats_for_test++;
      }
      /* PN: 07/05/2007. Process network device statistics */
      if(!xmlStrcmp(stats->name,(const xmlChar *)"netdev_stats")) { 
	if (rd->print_csv) open_csv_file(&(rd->csv_files.netdev_stats), "netdev_stats", test);
        process_netdev_stats(test_set, stats, test->id);
        stats_for_test++;
      }
      if(!xmlStrcmp(stats->name,(const xmlChar *)"test_stats")) {
        /* process test statistics */
	if (rd->print_csv) open_csv_file(&(rd->csv_files.test_stats), "test_stats", test);
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
      exit(-1);
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
  bsd_results_t *rd;
  double         confidence;
  double         temp;
  
  rd        = test_set->report_data;

  NETPERF_DEBUG_ENTRY(test_set->debug,test_set->where);

    /* calculate confidence and summary result values */
  confidence            = (test_set->get_confidence)(rd->run_time,
						     &(test_set->confidence),
						     &(rd->ave_time),
						     &(temp));
  rd->result_confidence = (test_set->get_confidence)(rd->results,
						     &(test_set->confidence),
						     &(rd->result_measured_mean),
						     &(rd->result_interval));
  if (test_set->debug) {
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
  if (test_set->debug) {
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
  if (test_set->debug) {
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
  if (test_set->debug) {
    fprintf(test_set->where,
            "\t%3drun confidence = %.2f%%\tcheck value = %f\n",
            test_set->confidence.count,
            100.0 * confidence, test_set->confidence.value);
    fflush(test_set->where);
  }
  NETPERF_DEBUG_EXIT(test_set->debug,test_set->where)
}

static void
print_run_results(tset_t *test_set)
{
  bsd_results_t *rd;
  FILE          *outfd;
  int            i;
  int            count;
  int            index;

#define HDR_LINES 3
  char *field1[HDR_LINES]   = { "INST",  "NUM",  " "       };   /* 4 */
  char *field2[HDR_LINES]   = { "SET",   "Name", " "       };   /* 6 */
  char *field3[HDR_LINES]   = { "RUN",   "Time", "sec"     };   /* 6 */
  char *field4A[HDR_LINES]  = { "DATA",  "RATE", "mb/s"    };   /* 7 */
  char *field4B[HDR_LINES]  = { "TRANS", "RATE", "tran/s"  };   /* 7 */
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
    if (rd->sd_denominator != 1.0) {
      fprintf(outfd,"%7s ",field4A[i]);                       /* 19,8 */
      fprintf(outfd,"%7s ",field5[i]);                        /* 27,8 */
      fprintf(outfd,"%7s ",field6[i]);                        /* 35,8 */
    }
    else {
      fprintf(outfd,"%10s ",field4B[i]);                      /* 19,11 */
    }
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
  if (rd->sd_denominator != 1.0) {
    fprintf(outfd,"%7.2f ",rd->results[index]);               /* 19,8 */
    fprintf(outfd,"%7.2f ",rd->xmit_results[index]);          /* 27,8 */
    fprintf(outfd,"%7.2f ",rd->recv_results[index]);          /* 35,8 */
  } 
  else {
    fprintf(outfd,"%10.2f ",rd->results[index]);              /* 19,11*/
  }
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
  bsd_results_t *rd;
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
  bsd_results_t *rd;
  FILE          *outfd;
  int            i;

#define HDR_LINES 3
                                                                /* field
                                                                   width*/
  char *field1[HDR_LINES]   = { "AVE",   "Over", "Num"     };   /* 4 */
  char *field2[HDR_LINES]   = { "SET",   "Name", " "       };   /* 6 */
  char *field3[HDR_LINES]   = { "TEST",  "Time", "sec"     };   /* 6 */
  char *field4A[HDR_LINES]  = { "DATA",  "RATE", "mb/s"    };   /* 7 */
  char *field5A[HDR_LINES]  = { "conf",  "+/-",  "mb/s"    };   /* 7 */
  char *field6A[HDR_LINES]  = { "Min",   "Rate", "mb/s"    };   /* 7 */
  char *field7A[HDR_LINES]  = { "Max",   "Rate", "mb/s"    };   /* 7 */
  char *field8[HDR_LINES]   = { "CPU",   "Util", "%/100"   };   /* 6 */
  char *field9[HDR_LINES]   = { "+/-",   "Util", "%/100"   };   /* 6 */
  char *field10A[HDR_LINES] = { "SD",    "usec", "/KB"     };   /* 6 */
  char *field11A[HDR_LINES] = { "+/-",   "usec", "/KB"     };   /* 6 */

  char *field4B[HDR_LINES]  = { "TRANS", "RATE", "tran/s"  };   /* 7 */
  char *field5B[HDR_LINES]  = { "conf",  "+/-",  "tran/s"  };   /* 7 */
  char *field6B[HDR_LINES]  = { "Min",   "Rate", "tran/s"  };   /* 7 */
  char *field7B[HDR_LINES]  = { "Max",   "Rate", "tran/s"  };   /* 7 */

  char *field10B[HDR_LINES] = { "SD",    "usec", "/tran"   };   /* 6 */
  char *field11B[HDR_LINES] = { "+/-",   "usec", "/tran"   };   /* 6 */

  char *field4[HDR_LINES];
  char *field5[HDR_LINES];
  char *field6[HDR_LINES];
  char *field7[HDR_LINES];

  char *field10[HDR_LINES];
  char *field11[HDR_LINES];
  
  rd    = test_set->report_data;
  outfd = rd->outfd;

  if (rd->sd_denominator != 1.0) {
    for (i = 0; i < HDR_LINES; i++) {
      field4[i]  = field4A[i];
      field5[i]  = field5A[i];
      field6[i]  = field6A[i];
      field7[i]  = field7A[i];
      field10[i] = field10A[i];
      field11[i] = field11A[i];
    }
  }
  else {
    for (i = 0; i < HDR_LINES; i++) {
      field4[i]  = field4B[i];
      field5[i]  = field5B[i];
      field6[i]  = field6B[i];
      field7[i]  = field7B[i];
      field10[i] = field10B[i];
      field11[i] = field11B[i];
    }
  }

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
  if (rd->sd_denominator != 1.0) {
    fprintf(outfd,"%7.2f ",rd->result_measured_mean);             /* 19,8 */
    fprintf(outfd,"%7.3f ",rd->result_confidence);                /* 27,8 */
    fprintf(outfd,"%7.2f ",rd->result_minimum);                   /* 35,8 */
    fprintf(outfd,"%7.2f ",rd->result_maximum);                   /* 43,8 */
  }
  else {
    fprintf(outfd,"%7.0f ",rd->result_measured_mean);             /* 19,8 */
    fprintf(outfd,"%7.2f ",rd->result_interval);                  /* 27,8 */
    fprintf(outfd,"%7.0f ",rd->result_minimum);                   /* 35,8 */
    fprintf(outfd,"%7.0f ",rd->result_maximum);                   /* 43,8 */
  }
  fprintf(outfd,"%6.4f ",rd->cpu_util_measured_mean);             /* 51,7 */
  fprintf(outfd,"%6.4f ",rd->cpu_util_interval);                  /* 58,7 */
  fprintf(outfd,"%6.3f ",rd->service_demand_measured_mean);       /* 65,7 */
  fprintf(outfd,"%6.3f ",rd->service_demand_interval);            /* 72,7 */
  fprintf(outfd,"\n");                                            /* 79,1 */
  fflush(outfd);
}

void
report_bsd_test_results(tset_t *test_set, char *report_flags, char *output)
{
  bsd_results_t *rd;
  int count;
  int max_count;
  int min_count;

  rd  = test_set->report_data;

  if (rd == NULL) {
    bsd_test_results_init(test_set,report_flags,output);
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
    /* PN: Close CSV files.
     */
    if (rd->print_csv) {
      if (test_set->debug)
        fprintf(test_set->where, "%s: closing CSV files..\n",__func__);
      close_csv_files(rd);
    }
  }


} /* end of report_bsd_test_results */

