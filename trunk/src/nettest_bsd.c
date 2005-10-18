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
@(#)nettest_bsd.c (c) Copyright 2005 Hewlett-Packard Co. Version 4.0.0";
#else
#define DIRTY
#define HISTOGRAM
#define INTERVALS
#endif /* lint */

#ifdef DIRTY
#define MAKE_DIRTY(mydata,ring)  /* DIRTY is not currently supported */
#else
#define MAKE_DIRTY(mydata,ring)  /* DIRTY is not currently supported */
#endif

#ifdef HISTOGRAM
#define HISTOGRAM_VARS        struct timeval time_one,time_two
#define HIST_TIMESTAMP(time)  gettimeofday(time,NULL)
#define HIST_ADD(h,delta)     HIST_add(h,delta)
#else
#define HISTOGRAM_VARS       /* variable declarations for histogram go here */
#define HIST_TIMESTAMP(time) /* time stamp call for histogram goes here */
#define HIST_ADD(h,delta)    /* call to add data to histogram goes here */
#endif

/****************************************************************/
/*                                                              */
/*      nettest_bsd.c                                           */
/*                                                              */
/*      the BSD sockets parsing routine...                      */
/*       ...with the addition of Windows NT, this is now also   */
/*          a Winsock test... sigh :)                           */
/*                                                              */
/*      scan_sockets_args()                                     */
/*                                                              */
/*      the actual test routines...                             */
/*                                                              */
/*      send_tcp_stream()       perform a tcp stream test       */
/*      recv_tcp_stream()       catch a tcp stream test         */
/*                                                              */
/****************************************************************/



#include <values.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef OFF
#include <netinet/in.h>
#endif

#include <sys/socket.h>
#include <netdb.h>

#include "netperf.h"

static int debug = 0;
FILE *where;

#ifdef HISTOGRAM
#include "hist.h"
#else
#define HIST  void*
#endif /* HISTOGRAM */

#include "nettest_bsd.h"

#ifdef WIN32
#define CHECK_FOR_INVALID_SOCKET (temp_socket == INVALID_SOCKET)
#define CHECK_FOR_RECV_ERROR(len) (len == SOCKET_ERROR)
#define CHECK_FOR_SEND_ERROR(len) (len >=0) || (len == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) 
#define GET_ERRNO WSAGetLastError()
#else
#define CHECK_FOR_INVALID_SOCKET (temp_socket < 0)
#define CHECK_FOR_RECV_ERROR(len) (len < 0)
#define CHECK_FOR_SEND_ERROR(len) (len >=0) || (errno == EINTR)
#define GET_ERRNO errno
#endif

/* Macros for accessing fields in the global netperf structures. */
#define SET_TEST_STATE(state)             test->new_state = state
#define GET_TEST_STATE                    test->new_state
#define CHECK_REQ_STATE                   test->state_req 
#define GET_TEST_DATA(test)               test->test_specific_data


static void
report_test_failure(test, function, err_code, err_string)
  test_t *test;
  char   *function;
  int     err_code;
  char   *err_string;
{
  if (debug) {
    fprintf(where,"%s: called report_test_failure:",function);
    fprintf(where,"reporting  %s  errno = %d\n",err_string,GET_ERRNO);
    fflush(where);
  }
  test->err_rc    = err_code;
  test->err_fn    = function;
  test->err_str   = err_string;
  test->new_state = TEST_ERROR;
  test->err_no    = GET_ERRNO;
}

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
            "\tflags: % family: %d: socktype: %d protocol %d addrlen %d\n",
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

static void
get_dependency_data(test_t *test, int type, int protocol)
{
  bsd_data_t *my_data = test->test_specific_data;

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
      if (debug) {
        fprintf(where,"Sleeping on getaddrinfo EAI_AGAIN\n");
        fflush(where);
      }
      sleep(1);
    }
  } while ((error == EAI_AGAIN) && (count <= 5));
    
  if (debug) {
    dump_addrinfo(where, remote_ai, remotehost, remoteport, remotefam);
  }

  if (!error) {
    my_data->remaddr = remote_ai;
  } else {
    if (debug) {
      fprintf(where,"get_dependency_data: getaddrinfo returned %d %s\n",
              error, gai_strerror(error));
      fflush(where);
    }
    report_test_failure(test,
                        "get_dependency_data",
                        BSDE_GETADDRINFO_ERROR,
                        gai_strerror(error));
  }
}


static void
set_dependent_data(test)
  test_t *test;
{
  
  bsd_data_t  *my_data = test->test_specific_data;
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
    } else {
      report_test_failure(test,
                          "set_dependent_data",
                          BSDE_XMLSETPROP_ERROR,
                          "error setting properties for dependency data");
    }
  } else {
    report_test_failure(test,
                        "set_dependent_data",
                        BSDE_XMLNEWNODE_ERROR,
                        "error getting new node for dependency data");
  }
}


unsigned int
convert(string,units)
  char *string;
  char *units;
{
  unsigned int base;
  base = atoi(string);
  if (strstr(units,"B")) {
    base *= 1;
  }
  if (strstr(units,"KB")) {
    base *= 1024;
  }
  if (strstr(units,"MB")) {
    base *= (1024 * 1024);
  }
  if (strstr(units,"GB")) {
    base *= (1024 * 1024 * 1024);
  }
  if (strstr(units,"kB")) {
    base *= (1000);
  }
  if (strstr(units,"mB")) {
    base *= (1000 * 1000);
  }
  if (strstr(units,"gB")) {
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
    temp_link->buffer_base = (char *)malloc(malloc_size);
    temp_link->buffer_ptr = (char *)(( (long)(temp_link->buffer_base) +
                          (long)alignment - 1) &
                         ~((long)alignment - 1));
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
static int
create_data_socket(test)
  test_t *test;
{
  bsd_data_t  *my_data = test->test_specific_data;

  int family           = my_data->locaddr->ai_family;
  int type             = my_data->locaddr->ai_socktype;
  int lss_size         = my_data->send_buf_size;
  int lsr_size         = my_data->recv_buf_size;
  int loc_nodelay      = my_data->no_delay;
  int loc_sndavoid     = my_data->send_avoid;
  int loc_rcvavoid     = my_data->recv_avoid;

  int temp_socket;
  int one;
  int sock_opt_len;

  /*set up the data socket                        */
  temp_socket = socket(family,
                       type,
                       0);

  if (CHECK_FOR_INVALID_SOCKET) {
    report_test_failure(test,
                        "create_data_socket",
                        BSDE_SOCKET_ERROR,
                        "error creating socket");
    return(temp_socket);
  }

  if (debug) {
    fprintf(where,"create_data_socket: socket %d obtained...\n",temp_socket);
    fflush(where);
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
                          "create_data_socket",
                          BSDE_SETSOCKOPT_ERROR,
                          "error setting local send socket buffer size");
      return(temp_socket);
    }
    if (debug > 1) {
      fprintf(where,
              "nettest_bsd: create_data_socket: SO_SNDBUF of %d requested.\n",
              lss_size);
      fflush(where);
    }
  }

  if (lsr_size > 0) {
    if(setsockopt(temp_socket, SOL_SOCKET, SO_RCVBUF,
                  &lsr_size, sizeof(int)) < 0) {
      report_test_failure(test,
                          "create_data_socket",
                          BSDE_SETSOCKOPT_ERROR,
                          "error setting local recv socket buffer size");
      return(temp_socket);
    }
    if (debug > 1) {
      fprintf(where,
              "nettest_bsd: create_data_socket: SO_RCVBUF of %d requested.\n",
              lsr_size);
      fflush(where);
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
    fprintf(where,
        "nettest_bsd: create_data_socket: getsockopt SO_SNDBUF: errno %d\n",
        errno);
    fflush(where);
    lss_size = -1;
  }
  if (getsockopt(temp_socket,
                 SOL_SOCKET,
                 SO_RCVBUF,
                 (char *)&lsr_size,
                 &sock_opt_len) < 0) {
    fprintf(where,
        "nettest_bsd: create_data_socket: getsockopt SO_RCVBUF: errno %d\n",
        errno);
    fflush(where);
    lsr_size = -1;
  }
  if (debug) {
    fprintf(where,
            "nettest_bsd: create_data_socket: socket sizes determined...\n");
    fprintf(where,
            "                       send: %d recv: %d\n",
            lss_size,lsr_size);
    fflush(where);
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
      fprintf(where,
              "nettest_bsd: create_data_socket: Could not enable receive copy avoidance");
      fflush(where);
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
      fprintf(where,
        "netperf: create_data_socket: Could not enable send copy avoidance\n");
      fflush(where);
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
  if (loc_nodelay) {
    one = 1;
    if(setsockopt(temp_socket,
                  getprotobyname("tcp")->p_proto,
                  TCP_NODELAY,
                  (char *)&one,
                  sizeof(one)) < 0) {
      fprintf(where,
              "netperf: create_data_socket: nodelay: errno %d\n",
              errno);
      fflush(where);
    }

    if (debug > 1) {
      fprintf(where,
              "netperf: create_data_socket: TCP_NODELAY requested...\n");
      fflush(where);
    }
  }
#else /* TCP_NODELAY */

  loc_nodelay = 0;

#endif /* TCP_NODELAY */

  return(temp_socket);

}




void
bsd_test_free(test_t *test)
{
  free(test->test_specific_data);
  test->test_specific_data = NULL;
}

bsd_data_t *
bsd_test_init(test_t *test, int type, int protocol)
{
  bsd_data_t *new_data;
  xmlNodePtr  args;
  xmlChar    *string;
  xmlChar    *units;
  xmlChar    *localhost;
  xmlChar    *localport;
  int         localfam;
  FILE       *fill_source;

  int               count;
  int               error;
  struct addrinfo   hints;
  struct addrinfo  *local_ai;
  struct addrinfo  *local_temp;

  where = stderr;

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
 
  if (args != NULL) {
    /* zero the bsd test specific data structure */
    memset(new_data,0,sizeof(bsd_data_t));

    string =  xmlGetProp(args,(const xmlChar *)"fill_file");
    /* fopen the fill file it will be used when allocating buffer rings */
    fill_source = fopen((char *)string,"r");
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

    string =  xmlGetProp(args,(const xmlChar *)"port_min");
    new_data->port_min = atoi((char *)string);

    string =  xmlGetProp(args,(const xmlChar *)"port_max");
    new_data->port_max = atoi((char *)string);

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

    /* now get and initialize the local the local addressing info */
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
        if (debug) {
          fprintf(where,"Sleeping on getaddrinfo EAI_AGAIN\n");
          fflush(where);
        }
        sleep(1);
      }
    } while ((error == EAI_AGAIN) && (count <= 5));
    
    if (debug) {
      dump_addrinfo(where, local_ai, localhost, localport, localfam);
    }

    if (!error) {
      new_data->locaddr = local_ai;
    } else {
      if (debug) {
        fprintf(where,"bsd_test_init: getaddrinfo returned %d %s\n",
                error, gai_strerror(error));
        fflush(where);
      }
      report_test_failure(test,
                          "bsd_test_init",
                          BSDE_GETADDRINFO_ERROR,
                          gai_strerror(error));
    }
  } else {
    report_test_failure(test,
                        "bsd_test_init",
                        BSDE_NO_SOCKET_ARGS,
                        "no socket_arg element was found");
  }

  test->test_specific_data = new_data;
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

static void
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
}

void
bsd_test_decode_stats(xmlNodePtr stats,test_t *test)
{
  xmlChar *value;
  xmlChar *name;
 
  if (stats && test) {
    name = NULL;
  }
}

static xmlNodePtr
bsd_test_get_stats(test_t *test)
{
  xmlNodePtr  stats = NULL;
  xmlAttrPtr  ap    = NULL;
  int         i,j;
  char        value[32];
  char        name[32];
  uint64_t    loc_cnt[BSD_MAX_COUNTERS];

  bsd_data_t *my_data = GET_TEST_DATA(test);

  if (debug) {
    fprintf(where,"bsd_test_get_stats: entered for %s test %s\n",
            test->id, test->test_name);
    fflush(where);
  }
  if ((stats = xmlNewNode(NULL,(xmlChar *)"test_stats")) != NULL) {
    /* set the properites of the test_stats message -
       the tid and time stamps/values and counter values  sgb 2004-07-07 */

    ap = xmlSetProp(stats,(xmlChar *)"tid",test->id);
    for (i = 0; i < BSD_MAX_COUNTERS; i++) {
      loc_cnt[i] = my_data->stats.counter[i];
      if (debug) {
        fprintf(where,"BSD_COUNTER%X = %#llx\n",i,loc_cnt[i]);
      } 
    }
    if (GET_TEST_STATE == TEST_MEASURE) {
      gettimeofday(&(my_data->curr_time), NULL);
      if (ap != NULL) {
        sprintf(value,"%#ld",my_data->curr_time.tv_sec);
        ap = xmlSetProp(stats,(xmlChar *)"time_sec",(xmlChar *)value);
        if (debug) {
          fprintf(where,"time_sec=%s\n",value);
          fflush(where);
        }
      }
      if (ap != NULL) {
        sprintf(value,"%#ld",my_data->curr_time.tv_usec);
        ap = xmlSetProp(stats,(xmlChar *)"time_usec",(xmlChar *)value);
        if (debug) {
          fprintf(where,"time_usec=%s\n",value);
          fflush(where);
        }
      }
    } else {
      if (ap != NULL) {
        sprintf(value,"%#ld",my_data->elapsed_time.tv_sec);
        ap = xmlSetProp(stats,(xmlChar *)"elapsed_sec",(xmlChar *)value);
        if (debug) {
          fprintf(where,"elapsed_sec=%s\n",value);
          fflush(where);
        }
      }
      if (ap != NULL) {
        sprintf(value,"%#ld",my_data->elapsed_time.tv_usec);
        ap = xmlSetProp(stats,(xmlChar *)"elapsed_usec",(xmlChar *)value);
        if (debug) {
          fprintf(where,"elapsed_usec=%s\n",value);
          fflush(where);
        }
      }
    }
    for (i = 0; i < BSD_MAX_COUNTERS; i++) {
      if (ap == NULL) {
        break;
      }
      if (loc_cnt[i]) {
        sprintf(value,"%#llx",my_data->stats.counter[i]);
        sprintf(name,"cntr%1X_value",i);
        ap = xmlSetProp(stats,(xmlChar *)name,(xmlChar *)value);
        if (debug) {
          fprintf(where,"%s=%s\n",name,value);
          fflush(where);
        }
      }
    }
    if (ap == NULL) {
      xmlFreeNode(stats);
      stats == NULL;
    }
  }
  if (debug) {
    fprintf(where,"bsd_test_get_stats: exiting for %s test %s\n",
            test->id, test->test_name);
    fflush(where);
  }
  return(stats);
}

void
recv_tcp_stream_clear_stats(test_t *test)
{
  bsd_data_t *my_data = GET_TEST_DATA(test);
  bsd_test_clear_stats(my_data);
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


void
send_tcp_stream_clear_stats(test_t *test)
{
  bsd_data_t *my_data = GET_TEST_DATA(test);
  bsd_test_clear_stats(my_data);
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
  bsd_data_t      *my_data;
  ring_elt_ptr     recv_ring;
  int              s_listen;
  int              s_data; 
  int              recv_size; 
  int              len; 
  int              rc; 
  int              firsttime = 1;
  struct sockaddr  myaddr;
  int              mylen=sizeof(myaddr);
  struct sockaddr  peeraddr;
  int              peerlen=sizeof(peeraddr);

  HISTOGRAM_VARS;

  /* create data socket s_listen */
  /* bind to get addressing information */
  /*      need to pass in IP if multi-homed      */
  /* listen for connection on s_listen */
  /* call getsockname to set the port  */
  /* report dependency data IP, port & family */
  /* accept connection */

  my_data = bsd_test_init(test, SOCK_STREAM, IPPROTO_TCP);

  my_data->recv_ring = allocate_buffer_ring(my_data->recv_width,
                                            my_data->recv_size,
                                            my_data->recv_align,
                                            my_data->recv_offset,
                                            my_data->fill_source);

  s_listen = create_data_socket(test);
  

  while ((GET_TEST_STATE != TEST_ERROR) &&
         (GET_TEST_STATE != TEST_DEAD)) {
    switch (GET_TEST_STATE) {
    case TEST_PREINIT:
      if (firsttime) {
        firsttime = 0;
        recv_ring = my_data->recv_ring;
        recv_size = my_data->recv_size;
        if (debug) {
          dump_addrinfo(where, my_data->locaddr,
                        (xmlChar *)NULL, (xmlChar *)NULL, -1);
        }
        if (debug) {
          fprintf(where,"recv_tcp_stream:create_data_socket returned %d\n",
                  s_listen);
          fflush(where);
        }
        rc = bind(s_listen, my_data->locaddr->ai_addr,
                   my_data->locaddr->ai_addrlen);
        if (debug) {
          fprintf(where,"recv_tcp_stream:bind returned %d  errno=%d\n",
                  rc,errno);
          fflush(where);
        }
        if (len == -1) {
          report_test_failure(test,
                              "recv_tcp_stream",
                              BSDE_BIND_FAILED,
                              "data socket bind failed");
        } else if (listen(s_listen,5) == -1) {
          report_test_failure(test,
                              "recv_tcp_stream",
                              BSDE_LISTEN_FAILED,
                              "data socket listen failed");
        } else if (getsockname(s_listen,
                               &myaddr,
                               &mylen) == -1) {
          report_test_failure(test,
                              "recv_tcp_stream",
                              BSDE_GETSOCKNAME_FAILED,
                              "getting the listen socket name failed");
        } else {
          memcpy(my_data->locaddr->ai_addr,&myaddr,mylen);
          my_data->locaddr->ai_addrlen = mylen;
          set_dependent_data(test);
          SET_TEST_STATE(TEST_INIT);
        }
      } else {
        report_test_failure(test,
                            "recv_tcp_stream",
                            BSDE_TEST_STATE_CORRUPTED,
                            "test found in ILLEGAL TEST_PREINIT state");
      }
      break;
    case TEST_INIT:
      if (CHECK_REQ_STATE == TEST_IDLE) {
        peerlen = sizeof(peeraddr);
        if (debug) {
          fprintf(where,"recv_tcp_stream:waiting in accept\n");
          fflush(where);
        }
        if ((s_data=accept(s_listen,
                          &peeraddr,
                          &peerlen)) == -1) {
          report_test_failure(test,
                              "recv_tcp_stream",
                              BSDE_ACCEPT_FAILED,
                              "listen socket accept failed");
        } else {
          if (debug) {
            fprintf(where,"recv_tcp_stream:accept returned successfully %d\n",
                    s_data);
            fflush(where);
          }
          SET_TEST_STATE(TEST_IDLE);
        }
      } else {
        report_test_failure(test,
                            "recv_tcp_stream",
                            BSDE_REQUESTED_STATE_INVALID,
                            "bad state requested for TEST_INIT state");
      }
      break;
    case TEST_IDLE:
      /* check for state transition */
      if (CHECK_REQ_STATE == TEST_IDLE) {
        sleep(1);
      } else if (CHECK_REQ_STATE == TEST_LOADED) {
        SET_TEST_STATE(TEST_LOADED);
      } else if (CHECK_REQ_STATE == TEST_DEAD) {
        SET_TEST_STATE(TEST_DEAD);
      } else {
        report_test_failure(test,
                            "recv_tcp_stream",
                            BSDE_REQUESTED_STATE_INVALID,
                            "in IDLE requested to go to illegal state");
      }
      break;
    case TEST_MEASURE:
      my_data->stats.named.bytes_received += len;
      my_data->stats.named.recv_calls++;
      /* check for state transition */
      if (CHECK_REQ_STATE != TEST_MEASURE) {
        if (CHECK_REQ_STATE == TEST_LOADED) {
          /* transitioning to loaded state from measure state 
             set current timestamp and update elapsed time */
          gettimeofday(&(my_data->curr_time), NULL);
          update_elapsed_time(my_data);
          SET_TEST_STATE(TEST_LOADED);
        } else {
          report_test_failure(test,
                              "recv_tcp_stream",
                              BSDE_REQUESTED_STATE_INVALID,
                              "in MEASURE requested to go to illegal state");
        }
      }
    case TEST_LOADED:
      /* code to make data dirty macro enabled by DIRTY */
      MAKE_DIRTY(my_data,recv_ring);
      /* code to timestamp enabled by HISTOGRAM */
      HIST_TIMESTAMP(&time_one);
      /* recv data for the test */
      if ((len=recv(s_data,
                    recv_ring->buffer_ptr,
                    recv_size,
                    0)) != 0) {
        /* this macro hides windows differences */
        if (CHECK_FOR_RECV_ERROR(len)) {
          report_test_failure(test,
                              "recv_tcp_stream",
                              BSDE_DATA_RECV_ERROR,
                              "data_recv_error");
        }
      }
      /* code to timestamp enabled by HISTOGRAM */
      HIST_TIMESTAMP(&time_two);
      HIST_ADD(my_data->time_hist,delta_macro(&time_one,&time_two));
      recv_ring = recv_ring->next;
      /* check for state transition */
      if ((GET_TEST_STATE == TEST_LOADED) && 
          (CHECK_REQ_STATE != TEST_LOADED)) {
        if (CHECK_REQ_STATE == TEST_MEASURE) {
          SET_TEST_STATE(TEST_MEASURE);
          /* transitioning to measure state from loaded state 
             set previous timestamp */
          gettimeofday(&(my_data->prev_time), NULL);
        } else if (CHECK_REQ_STATE == TEST_IDLE) {
          if (shutdown(s_data,1) == -1) {
            report_test_failure(test,
                                "recv_tcp_stream",
                                BSDE_SOCKET_SHUTDOWN_FAILED,
                                "failure shuting down data socket");
          } else {
            close(s_data);
            peerlen = sizeof(peeraddr);
            if (debug) {
              fprintf(where,"recv_tcp_stream:waiting in accept\n");
              fflush(where);
            }
            if ((s_data=accept(s_listen,
                              &peeraddr,
                              &peerlen)) == -1) {
              report_test_failure(test,
                                  "recv_tcp_stream",
                                  BSDE_ACCEPT_FAILED,
                                  "listen socket accept failed");
            } else {
              if (debug) {
                fprintf(where,
                        "recv_tcp_stream:accept returned successfully %d\n",
                        s_data);
                fflush(where);
              }
              SET_TEST_STATE(TEST_IDLE);
            }
          }
        } else {
          report_test_failure(test,
                              "recv_tcp_stream",
                              BSDE_REQUESTED_STATE_INVALID,
                              "in LOADED requested to go to illegal state");
        }
      }
      break;
    default:
      report_test_failure(test,
                          "recv_tcp_stream",
                          BSDE_TEST_STATE_CORRUPTED,
                          "test found in ILLEGAL state");
    } /* end of switch in while loop */
  } /* end of while for test */

  /* perform a shutdown to signal the sender that */
  /* we have received all the data sent. raj 4/93 */
  shutdown(s_data,1);

  close(s_listen);
  close(s_data);

  while (GET_TEST_STATE != TEST_DEAD) {
    if (CHECK_REQ_STATE == TEST_DEAD) {
        bsd_test_free(test);
        SET_TEST_STATE(TEST_DEAD);
    }
  }
}


/* This routine implements the TCP unidirectional data transfer test */
/* (a.k.a. stream) for the sockets interface. It receives its */
/* parameters via the xml node contained in the test structure */
/* output to the standard output. */
/* results are collected by the procedure send_tcp_stream_get_stats */
void
send_tcp_stream(test_t *test)
{
  bsd_data_t    *my_data;
  ring_elt_ptr   send_ring;
  int            send_socket; 
  int            len;
  int            send_size;
  int            firsttime = 1;

  HISTOGRAM_VARS;

  /* setup server with remote ip address and family*/
  /* create_data_socket AF_INET,SOCK_STREAM */
  /* connect using created data socket and server structure for remote */

  my_data = bsd_test_init(test, SOCK_STREAM, IPPROTO_TCP);

  my_data->send_ring = allocate_buffer_ring(my_data->send_width,
                                            my_data->send_size,
                                            my_data->send_align,
                                            my_data->send_offset,
                                            my_data->fill_source);

  get_dependency_data(test, SOCK_STREAM, IPPROTO_TCP);
  send_socket = create_data_socket(test);
  
  while ((GET_TEST_STATE != TEST_ERROR) &&
         (GET_TEST_STATE != TEST_DEAD)) {
    switch (GET_TEST_STATE) {
    case TEST_PREINIT:
      if (firsttime) {
        firsttime = 0;
        send_ring = my_data->send_ring;
        send_size = my_data->send_size;
        SET_TEST_STATE(TEST_INIT);
      } else {
        report_test_failure(test,
                            "send_tcp_stream",
                            BSDE_TEST_STATE_CORRUPTED,
                            "test found in ILLEGAL TEST_PREINIT state");
      }
      break;
    case TEST_INIT:
      if (CHECK_REQ_STATE == TEST_IDLE) {
        if (debug) {
          fprintf(where,"send_tcp_stream: in INIT state making connect call\n");
          fflush(where);
        }
        if (connect(send_socket,
                    my_data->remaddr->ai_addr,
                    my_data->remaddr->ai_addrlen) < 0) {
          report_test_failure(test,
                              "send_tcp_stream",
                              BSDE_CONNECT_FAILED,
                              "data socket connect failed");
        } else {
          if (debug) {
            fprintf(where,"send_tcp_stream: connected and moving to IDLE\n");
            fflush(where);
          }
          SET_TEST_STATE(TEST_IDLE);
        }
      } else {
        report_test_failure(test,
                            "send_tcp_stream",
                            BSDE_REQUESTED_STATE_INVALID,
                            "test found in TEST_INIT state");
      }
      break;
    case TEST_IDLE:
      /* check for state transition */
      if (CHECK_REQ_STATE == TEST_IDLE) {
        sleep(1);
      } else if (CHECK_REQ_STATE == TEST_LOADED) {
        SET_TEST_STATE(TEST_LOADED);
      } else if (CHECK_REQ_STATE == TEST_DEAD) {
        SET_TEST_STATE(TEST_DEAD);
      } else {
        report_test_failure(test,
                            "send_tcp_stream",
                            BSDE_REQUESTED_STATE_INVALID,
                            "in IDLE requested to move to illegal state");
      }
      break;
    case TEST_MEASURE:
      my_data->stats.named.bytes_sent += send_size;
      my_data->stats.named.send_calls++;
      /* check for state transition */
      if (CHECK_REQ_STATE != TEST_MEASURE) {
        if (CHECK_REQ_STATE == TEST_LOADED) {
          /* transitioning to loaded state from measure state 
             set current timestamp and update elapsed time */
          gettimeofday(&(my_data->curr_time), NULL);
          update_elapsed_time(my_data);
          SET_TEST_STATE(TEST_LOADED);
        } else {
          report_test_failure(test,
                              "send_tcp_stream",
                              BSDE_REQUESTED_STATE_INVALID,
                              "in MEASURE requested to move to illegal state");
        }
      }
    case TEST_LOADED:
      /* code to make data dirty macro enabled by DIRTY */
      MAKE_DIRTY(my_data,send_ring);
      /* code to timestamp enabled by HISTOGRAM */
      HIST_TIMESTAMP(&time_one);
      /* send data for the test */
      if((len=send(send_socket,
                   send_ring->buffer_ptr,
                   send_size,
                   0)) != send_size) {
        /* this macro hides windows differences */
        if ((CHECK_FOR_SEND_ERROR(len)) &&
            (CHECK_REQ_STATE != TEST_IDLE)) {
          report_test_failure(test,
                              "send_tcp_stream",
                              BSDE_DATA_SEND_ERROR,
                              "data_send_error");
        }
      }
      /* code to timestamp enabled by HISTOGRAM */
      HIST_TIMESTAMP(&time_two);
      HIST_ADD(my_data->time_hist,delta_macro(&time_one,&time_two));
      send_ring = send_ring->next;
      /* check for state transition */
      if ((GET_TEST_STATE == TEST_LOADED) && 
          (CHECK_REQ_STATE != TEST_LOADED)) {
        if (CHECK_REQ_STATE == TEST_MEASURE) {
          SET_TEST_STATE(TEST_MEASURE);
          /* transitioning to measure state from loaded state 
             set previous timestamp */
          gettimeofday(&(my_data->prev_time), NULL);
        } else if (CHECK_REQ_STATE == TEST_IDLE) {
          if (shutdown(send_socket,1) == -1) {
            report_test_failure(test,
                                "send_tcp_stream",
                                BSDE_SOCKET_SHUTDOWN_FAILED,
                                "failure shuting down data socket");
          } else {
            recv(send_socket, send_ring->buffer_ptr, send_size, 0);
            close(send_socket);
            send_socket = create_data_socket(test);
            if (GET_TEST_STATE != TEST_ERROR) {
              if (debug) {
                fprintf(where,"send_tcp_stream: connecting from LOAD state\n");
                fflush(where);
              }
              if (connect(send_socket,
                          my_data->remaddr->ai_addr,
                          my_data->remaddr->ai_addrlen) < 0) {
                report_test_failure(test,
                                    "send_tcp_stream",
                                    BSDE_CONNECT_FAILED,
                                    "data socket connect failed");
              } else {
                if (debug) {
                  fprintf(where,"send_tcp_stream: connected moving to IDLE\n");
                  fflush(where);
                }
                SET_TEST_STATE(TEST_IDLE);
              }
            }
          }
        } else {
          report_test_failure(test,
                              "send_tcp_stream",
                              BSDE_REQUESTED_STATE_INVALID,
                              "in LOADED requested to move to illegal state");
        }
      }
      break;
    default:
      report_test_failure(test,
                          "send_tcp_stream",
                          BSDE_TEST_STATE_CORRUPTED,
                          "test found in ILLEGAL state");
    } /* end of switch in while loop */
  } /* end of while for test */

  close(send_socket);

  while (GET_TEST_STATE != TEST_DEAD) {
    if (CHECK_REQ_STATE == TEST_DEAD) {
      bsd_test_free(test);
      SET_TEST_STATE(TEST_DEAD);
    }
  }

}  /* end of send_tcp_stream */



void
recv_tcp_rr_clear_stats(test_t *test)
{
  bsd_data_t *my_data = GET_TEST_DATA(test);
  bsd_test_clear_stats(my_data);
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


void
send_tcp_rr_clear_stats(test_t *test)
{
  bsd_data_t *my_data = GET_TEST_DATA(test);
  bsd_test_clear_stats(my_data);
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
  bsd_data_t      *my_data;
  ring_elt_ptr     recv_ring;
  ring_elt_ptr     send_ring;
  char *           req_ptr;
  int              s_listen;
  int              s_data; 
  int              bytes_left; 
  int              recv_size; 
  int              send_size; 
  int              len; 
  int              rc;
  int              firsttime = 1;
  int              go_on = 1;
  struct sockaddr  myaddr;
  int              mylen=sizeof(myaddr);
  struct sockaddr  peeraddr;
  int              peerlen=sizeof(peeraddr);
  int              loc_debug = 0;

  HISTOGRAM_VARS;

  /* create data socket s_listen */
  /* bind to get addressing information */
  /*      need to pass in IP if multi-homed      */
  /* listen for connection on s_listen */
  /* call getsockname to set the port  */
  /* report dependency data IP, port & family */
  /* accept connection */

  my_data = bsd_test_init(test, SOCK_STREAM, IPPROTO_TCP);

  my_data->recv_ring = allocate_buffer_ring(my_data->recv_width,
                                            my_data->recv_size,
                                            my_data->recv_align,
                                            my_data->recv_offset,
                                            my_data->fill_source);

  my_data->send_ring = allocate_buffer_ring(my_data->send_width,
                                            my_data->send_size,
                                            my_data->send_align,
                                            my_data->send_offset,
                                            my_data->fill_source);

  s_listen = create_data_socket(test);
  

  while ((GET_TEST_STATE != TEST_ERROR) &&
         (GET_TEST_STATE != TEST_DEAD)) {
    switch (GET_TEST_STATE) {
    case TEST_PREINIT:
      if (firsttime) {
        firsttime = 0;
        recv_ring = my_data->recv_ring;
        recv_size = my_data->recv_size;
        send_ring = my_data->send_ring;
        send_size = my_data->send_size;
        if (debug) {
          dump_addrinfo(where, my_data->locaddr,
                        (xmlChar *)NULL, (xmlChar *)NULL, -1);
        }
        if (debug || loc_debug) {
          fprintf(where,"recv_tcp_rr:create_data_socket returned %d\n",
                  s_listen);
          fflush(where);
        }
        rc = bind(s_listen, my_data->locaddr->ai_addr,
                   my_data->locaddr->ai_addrlen);
        if (debug || loc_debug) {
          fprintf(where,"recv_tcp_rr:bind returned %d  errno=%d\n",
                  rc,errno);
          fflush(where);
        }
        if (len == -1) {
          report_test_failure(test,
                              "recv_tcp_rr",
                              BSDE_BIND_FAILED,
                              "data socket bind failed");
        } else if (listen(s_listen,5) == -1) {
          report_test_failure(test,
                              "recv_tcp_rr",
                              BSDE_LISTEN_FAILED,
                              "data socket listen failed");
        } else if (getsockname(s_listen,
                               &myaddr,
                               &mylen) == -1) {
          report_test_failure(test,
                              "recv_tcp_rr",
                              BSDE_GETSOCKNAME_FAILED,
                              "getting the listen socket name failed");
        } else {
          memcpy(my_data->locaddr->ai_addr,&myaddr,mylen);
          my_data->locaddr->ai_addrlen = mylen;
          set_dependent_data(test);
          SET_TEST_STATE(TEST_INIT);
        }
      } else {
        report_test_failure(test,
                            "recv_tcp_rr",
                            BSDE_TEST_STATE_CORRUPTED,
                            "test found in ILLEGAL TEST_PREINIT state");
      }
      break;
    case TEST_INIT:
      if (CHECK_REQ_STATE == TEST_IDLE) {
        peerlen = sizeof(peeraddr);
        if (debug || loc_debug) {
          fprintf(where,"recv_tcp_rr:waiting in accept\n");
          fflush(where);
        }
        if ((s_data=accept(s_listen,
                          &peeraddr,
                          &peerlen)) == -1) {
          report_test_failure(test,
                              "recv_tcp_rr",
                              BSDE_ACCEPT_FAILED,
                              "listen socket accept failed");
        } else {
          if (debug || loc_debug) {
            fprintf(where,"recv_tcp_rr:accept returned successfully %d\n",
                    s_data);
            fflush(where);
          }
          SET_TEST_STATE(TEST_IDLE);
        }
      } else {
        report_test_failure(test,
                            "recv_tcp_rr",
                            BSDE_REQUESTED_STATE_INVALID,
                            "bad state requested for TEST_INIT state");
      }
      break;
    case TEST_IDLE:
      /* check for state transition */
      if (CHECK_REQ_STATE == TEST_IDLE) {
        sleep(1);
      } else if (CHECK_REQ_STATE == TEST_LOADED) {
        SET_TEST_STATE(TEST_LOADED);
      } else if (CHECK_REQ_STATE == TEST_DEAD) {
        SET_TEST_STATE(TEST_DEAD);
      } else {
        report_test_failure(test,
                            "recv_tcp_rr",
                            BSDE_REQUESTED_STATE_INVALID,
                            "in IDLE requested to move to illegal state");
      }
      break;
    case TEST_MEASURE:
      my_data->stats.named.trans_received++;
      /* check for state transition */
      if (CHECK_REQ_STATE != TEST_MEASURE) {
        if (CHECK_REQ_STATE == TEST_LOADED) {
          /* transitioning to loaded state from measure state 
             set current timestamp and update elapsed time */
          gettimeofday(&(my_data->curr_time), NULL);
          update_elapsed_time(my_data);
          SET_TEST_STATE(TEST_LOADED);
        } else {
          report_test_failure(test,
                              "recv_tcp_rr",
                              BSDE_REQUESTED_STATE_INVALID,
                            "in MEASURE requested to move to illegal state");
        }
      }
    case TEST_LOADED:
      /* code to timestamp enabled by HISTOGRAM */
      HIST_TIMESTAMP(&time_one);
      /* recv the request for the test */
      req_ptr      = recv_ring->buffer_ptr;
      bytes_left   = recv_size;
      while (bytes_left > 0) {
        if ((len=recv(s_data,
                      req_ptr,
                      bytes_left,
                      0)) != 0) {
        /* this macro hides windows differences */
          if (CHECK_FOR_RECV_ERROR(len)) {
            report_test_failure(test,
                                "recv_tcp_rr",
                                BSDE_DATA_RECV_ERROR,
                                "data_recv_error");
            break;
          }
          req_ptr    += len;
          bytes_left -= len;
        } 
        if ((CHECK_REQ_STATE != TEST_LOADED) &&
            (CHECK_REQ_STATE != TEST_MEASURE)) {
          break;
        }
      }

      if ((CHECK_REQ_STATE == TEST_LOADED) ||
          (CHECK_REQ_STATE == TEST_MEASURE)) {
        if ((len=send(s_data,
                      send_ring->buffer_ptr,
                      send_size,
                      0)) != send_size) {
          /* this macro hides windows differences */
          if ((CHECK_FOR_SEND_ERROR(len)) &&
              ((CHECK_REQ_STATE != TEST_IDLE) ||
               (GET_TEST_STATE  != TEST_ERROR))) {
            report_test_failure(test,
                                "recv_tcp_rr",
                                BSDE_DATA_SEND_ERROR,
                                "data_send_error");
          }
        }
      }

      /* code to timestamp enabled by HISTOGRAM */
      HIST_TIMESTAMP(&time_two);
      HIST_ADD(my_data->time_hist,delta_macro(&time_one,&time_two));

      recv_ring = recv_ring->next;
      send_ring = send_ring->next;
                     
      /* check for state transition */
      if ((GET_TEST_STATE == TEST_LOADED) &&
          (CHECK_REQ_STATE != TEST_LOADED)) {
        if (CHECK_REQ_STATE == TEST_MEASURE) {
          SET_TEST_STATE(TEST_MEASURE);
          /* transitioning to measure state from loaded state 
             set previous timestamp */
          gettimeofday(&(my_data->prev_time), NULL);
        } else if (CHECK_REQ_STATE == TEST_IDLE) {
          if (shutdown(s_data,1) == -1) {
            report_test_failure(test,
                                "recv_tcp_rr",
                                BSDE_SOCKET_SHUTDOWN_FAILED,
                                "failure shuting down data socket");
          } else {
            recv(s_data, send_ring->buffer_ptr, send_size, 0);
            close(s_data);
            peerlen = sizeof(peeraddr);
            if (debug || loc_debug) {
              fprintf(where,"recv_tcp_rr:waiting in accept\n");
              fflush(where);
            }
            if ((s_data=accept(s_listen,
                              &peeraddr,
                              &peerlen)) == -1) {
              report_test_failure(test,
                                  "recv_tcp_rr",
                                  BSDE_ACCEPT_FAILED,
                                  "listen socket accept failed");
            } else {
              if (debug || loc_debug) {
                fprintf(where,"recv_tcp_rr:accept returned successfully %d\n",
                        s_data);
                fflush(where);
              }
              SET_TEST_STATE(TEST_IDLE);
            }
          }
        } else {
          report_test_failure(test,
                              "recv_tcp_rr",
                              BSDE_REQUESTED_STATE_INVALID,
                              "in LOADED requested to move to illegal state");
        }
      }
      break;
    default:
      report_test_failure(test,
                          "recv_tcp_rr",
                          BSDE_TEST_STATE_CORRUPTED,
                          "test found in ILLEGAL state");
    } /* end of switch in while loop */
  } /* end of while for test */

  /* perform a shutdown to signal the sender that */
  /* we have received all the data sent. raj 4/93 */
  shutdown(s_data,1);

  close(s_listen);
  close(s_data);

  while (GET_TEST_STATE != TEST_DEAD) {
    if (CHECK_REQ_STATE == TEST_DEAD) {
        bsd_test_free(test);
        SET_TEST_STATE(TEST_DEAD);
    }
  }
}


/* This routine implements the TCP request/responce test */
/* (a.k.a. rr) for the sockets interface. It receives its */
/* parameters via the xml node contained in the test structure */
/* output to the standard output. */
/* results are collected by the procedure send_tcp_rr_get_stats */
void
send_tcp_rr(test_t *test)
{
  bsd_data_t    *my_data;
  ring_elt_ptr   recv_ring;
  ring_elt_ptr   send_ring;
  char          *rsp_ptr;
  int            send_socket; 
  int            bytes_left;
  int            len;
  int            recv_size;
  int            send_size;
  int            firsttime = 1;

  HISTOGRAM_VARS;

  /* setup server with remote ip address and family*/
  /* create_data_socket AF_INET,SOCK_STREAM */
  /* connect using created data socket and server structure for remote */

  my_data = bsd_test_init(test, SOCK_STREAM, IPPROTO_TCP);

  my_data->recv_ring = allocate_buffer_ring(my_data->recv_width,
                                            my_data->recv_size,
                                            my_data->recv_align,
                                            my_data->recv_offset,
                                            my_data->fill_source);

  my_data->send_ring = allocate_buffer_ring(my_data->send_width,
                                            my_data->send_size,
                                            my_data->send_align,
                                            my_data->send_offset,
                                            my_data->fill_source);

  get_dependency_data(test, SOCK_STREAM, IPPROTO_TCP);
  send_socket = create_data_socket(test);
  
  while ((GET_TEST_STATE != TEST_ERROR) &&
         (GET_TEST_STATE != TEST_DEAD)) {
    switch (GET_TEST_STATE) {
    case TEST_PREINIT:
      if (firsttime) {
        firsttime = 0;
        recv_ring = my_data->recv_ring;
        recv_size = my_data->recv_size;
        send_ring = my_data->send_ring;
        send_size = my_data->send_size;
        SET_TEST_STATE(TEST_INIT);
      } else {
        report_test_failure(test,
                            "send_tcp_rr",
                            BSDE_TEST_STATE_CORRUPTED,
                            "test found in ILLEGAL TEST_PREINIT state");
      }
      break;
    case TEST_INIT:
      if (CHECK_REQ_STATE == TEST_IDLE) {
        if (debug) {
          fprintf(where,"send_tcp_rr: in INIT state making connect call\n");
          fflush(where);
        }
        if (connect(send_socket,
                    my_data->remaddr->ai_addr,
                    my_data->remaddr->ai_addrlen) < 0) {
          report_test_failure(test,
                              "send_tcp_rr",
                              BSDE_CONNECT_FAILED,
                              "data socket connect failed");
        } else {
          if (debug) {
            fprintf(where,"send_tcp_rr: connected and moving to IDLE\n");
            fflush(where);
          }
          SET_TEST_STATE(TEST_IDLE);
        }
      } else {
        report_test_failure(test,
                            "send_tcp_rr",
                            BSDE_REQUESTED_STATE_INVALID,
                            "test found in TEST_INIT state");
      }
      break;
    case TEST_IDLE:
      /* check for state transition */
      if (CHECK_REQ_STATE == TEST_IDLE) {
        sleep(1);
      } else if (CHECK_REQ_STATE == TEST_LOADED) {
        SET_TEST_STATE(TEST_LOADED);
        /* add code for DO_FIRST_BURST if desired */
      } else if (CHECK_REQ_STATE == TEST_DEAD) {
        SET_TEST_STATE(TEST_DEAD);
      } else {
        report_test_failure(test,
                            "send_tcp_rr",
                            BSDE_REQUESTED_STATE_INVALID,
                            "in IDLE requested to move to illegal state");
      }
      break;
    case TEST_MEASURE:
      my_data->stats.named.trans_sent++;
      /* check for state transition */
      if (CHECK_REQ_STATE != TEST_MEASURE) {
        if (CHECK_REQ_STATE == TEST_LOADED) {
          /* transitioning to loaded state from measure state 
             set current timestamp and update elapsed time */
          gettimeofday(&(my_data->curr_time), NULL);
          update_elapsed_time(my_data);
          SET_TEST_STATE(TEST_LOADED);
        } else {
          report_test_failure(test,
                              "send_tcp_rr",
                              BSDE_REQUESTED_STATE_INVALID,
                              "in MEASURE requested to move to illegal state");
        }
      }
    case TEST_LOADED:
      /* code to make data dirty macro enabled by DIRTY */
      MAKE_DIRTY(my_data,send_ring);
      /* code to timestamp enabled by HISTOGRAM */
      HIST_TIMESTAMP(&time_one);
      /* send data for the test */
      if((len=send(send_socket,
                   send_ring->buffer_ptr,
                   send_size,
                   0)) != send_size) {
        /* this macro hides windows differences */
        if ((CHECK_FOR_SEND_ERROR(len)) &&
            (CHECK_REQ_STATE != TEST_IDLE)) {
          report_test_failure(test,
                              "send_tcp_rr",
                              BSDE_DATA_SEND_ERROR,
                              "data_send_error");
        }
      }

      /* receive the response for the test */
      rsp_ptr     = recv_ring->buffer_ptr;
      bytes_left  = recv_size;
      while (bytes_left > 0) {
        if ((len=recv(send_socket,
                      rsp_ptr,
                      bytes_left,
                      0)) != 0) {
        /* this macro hides windows differences */
          if (CHECK_FOR_RECV_ERROR(len)) {
            report_test_failure(test,
                                "send_tcp_rr",
                                BSDE_DATA_RECV_ERROR,
                                "data_recv_error");
            break;
          }
          rsp_ptr    += len;
          bytes_left -= len;
        }
        if ((CHECK_REQ_STATE != TEST_LOADED) &&
            (CHECK_REQ_STATE != TEST_MEASURE)) {
          break;
        }
      }

      /* code to timestamp enabled by HISTOGRAM */
      HIST_TIMESTAMP(&time_two);
      HIST_ADD(my_data->time_hist,delta_macro(&time_one,&time_two));

      recv_ring = recv_ring->next;
      send_ring = send_ring->next;

      /* check for state transition */
      if ((GET_TEST_STATE == TEST_LOADED) &&
          (CHECK_REQ_STATE != TEST_LOADED)) {
        if (CHECK_REQ_STATE == TEST_MEASURE) {
          SET_TEST_STATE(TEST_MEASURE);
          /* transitioning to measure state from loaded state 
             set previous timestamp */
          gettimeofday(&(my_data->prev_time), NULL);
        } else if (CHECK_REQ_STATE == TEST_IDLE) {
          close(send_socket);
          send_socket = create_data_socket(test);
          if (GET_TEST_STATE != TEST_ERROR) {
            if (debug) {
              fprintf(where,"send_tcp_rr: connecting from LOAD state\n");
              fflush(where);
            }
            if (connect(send_socket,
                        my_data->remaddr->ai_addr,
                        my_data->remaddr->ai_addrlen) < 0) {
              report_test_failure(test,
                                  "send_tcp_rr",
                                  BSDE_CONNECT_FAILED,
                                  "data socket connect failed");
            } else {
              if (debug) {
                fprintf(where,"send_tcp_rr: connected moving to IDLE\n");
                fflush(where);
              }
              SET_TEST_STATE(TEST_IDLE);
            }
          }
        } else {
          report_test_failure(test,
                              "send_tcp_rr",
                              BSDE_REQUESTED_STATE_INVALID,
                              "in LOADED requested to move to illegal state");
        }
      }
      break;
    default:
      report_test_failure(test,
                          "send_tcp_rr",
                          BSDE_TEST_STATE_CORRUPTED,
                          "test found in ILLEGAL state");
    } /* end of switch in while loop */
  } /* end of while for test */

  close(send_socket);

  while (GET_TEST_STATE != TEST_DEAD) {
    if (CHECK_REQ_STATE == TEST_DEAD) {
      bsd_test_free(test);
      SET_TEST_STATE(TEST_DEAD);
    }
  }

} /* end of send_tcp_rr */


/*  This implementation of report_bsd_test_results will generate strange
    results if transaction count and throughput tests are included in the
    same test set. The first test in the set sets the headers and algorithm
    for computing service demand */

void
report_bsd_test_results(tset_t *test_set, char *report_flags, char *output)
{
  bsd_results_t *my_data   = test_set->report_data;
  int            loc_debug = 0;
  int            count            = test_set->confidence.count - 1;
  int            max_count        = test_set->confidence.max_count;
  int            min_count        = test_set->confidence.min_count;
  int            sys_stats_count  = 0;
  int            test_stats_count = 0;
  int            test_stats;
  double         sd_denominator   = 0.0;
  double         local_cpus;

  /* Summary report variables */
  double         result_measured_mean;
  double         xmit_measured_mean;
  double         recv_measured_mean;
  double         cpu_util_measured_mean;
  double         service_demand_measured_mean;
  double         confidence;
  double         result_confidence;
  double         xmit_confidence;
  double         recv_confidence;
  double         cpu_util_confidence;
  double         serv_demand_confidence;
  double         ave_time;

  /* Per Run report variables */
  double         acc_results;
  double         acc_xmit_rate;
  double         acc_recv_rate;
  double         service_demand;
  double         run_time;

  /* Per Test variables */
  double         elapsed_seconds;
  double         sys_seconds;
  double         sys_util;
  double         calibration;
  double         local_idle;
  double         local_busy;
  double         result;
  double         xmit_rate;
  double         recv_rate;
  double         xmit_trans_rate;
  double         recv_trans_rate;

  int            i;
  int            flags    = atoi(report_flags);
  int            last_hdr = 0;
  xmlNodePtr     stats;
  xmlNodePtr     prev_stats;
  FILE          *outfd = NULL;
  test_t        *test;
  tset_elt_t    *set_elt;

#define BSD_SUMMARY_FLAG   1
#define BSD_PER_RUN_FLAG   2
#define BSD_PER_TEST_FLAG  4
  
#define BSD_SUMMARY_HDR    1
#define BSD_PER_RUN_HDR    BSD_PER_RUN_FLAG
#define BSD_PER_TEST_HDR   BSD_PER_TEST_FLAG

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
    "user_count",
    "sys_count",
    "int_count"
  };

  flags = BSD_PER_TEST_FLAG + BSD_PER_RUN_FLAG;

  if (my_data == NULL) {
    if (output) {
      if (debug || loc_debug) {
        fprintf(where,"report_bsd_test_results: report going to file %s\n",
                output);
        fflush(where);
      }
      outfd = fopen(output,"a");
    } else {
      if (debug || loc_debug) {
        fprintf(where,"report_bsd_test_results: report going to file stdout\n");
        fflush(where);
      }
      outfd = stdout;
    }
    /* allocate and initialize report data if first time */
    my_data = malloc(sizeof(bsd_results_t) + 6 * max_count * sizeof(double));
    memset(my_data, 0,
           sizeof(sizeof(bsd_results_t) + 6 * max_count * sizeof(double)));
    my_data->max_count    = max_count;
    my_data->results      = &(my_data->results_start);
    my_data->xmit_results = &(my_data->results[max_count]);
    my_data->recv_results = &(my_data->xmit_results[max_count]);
    my_data->utilization  = &(my_data->recv_results[max_count]);
    my_data->servdemand   = &(my_data->utilization[max_count]);
    my_data->run_time     = &(my_data->servdemand[max_count]);
    my_data->outfd        = outfd;
  } else {
    outfd = my_data->outfd;
  }
  if (max_count != my_data->max_count) {
    /* someone is playing games don't generate report*/
    fprintf(where,"Max_count changed between invocations of %s\n",
            "report_bsd_test_results");
    fprintf(where,"exiting netperf now!!\n");
    fflush(where);
    exit;
  }
  
  /* process statistics for this run */
  acc_results = 0.0;
  acc_xmit_rate = 0.0;
  acc_recv_rate = 0.0;
  run_time      = 0.0;
  sys_seconds   = 0.0;
  sys_util      = 0.0;
  set_elt = test_set->tests;
  if (debug || loc_debug) {
   fprintf(where,"test_set %s has %d tests looking for statistics\n",
           test_set->id,test_set->num_tests);
   fflush(where);
  }
  while (set_elt != NULL) {
    int stats_for_test;

    test   = set_elt->test;
    stats  = test->received_stats->xmlChildrenNode;

    if (debug || loc_debug) {
      if (stats) {
        fprintf(where,"\ttest %s has '%s' statistics\n", test->id,stats->name);
      } else {
        fprintf(where,"\ttest %s has no statistics available!\n", test->id);
      }
      fflush(where);
    }

    stats_for_test = 0;
    while(stats != NULL) {
      /* process all the statistics records for this test */
      if (debug || loc_debug) {
        fprintf(where,"\tfound some statistics");
        fflush(where);
      }
      if(!xmlStrcmp(stats->name,(const xmlChar *)"sys_stats")) {
        /* process system statistics */
        test_stats = 0;
        if (debug || loc_debug) {
          fprintf(where,"\tprocessing sys_stats\n");
          fflush(where);
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
          } else {
            sys_cntr[i] = 0.0;
          }
          if (debug || loc_debug) {
            fprintf(where,"\t%12s sys_stats[%d] = %10g '%s'\n",
                    sys_cntr_name[i], i, sys_cntr[i],
                    xmlGetProp(stats, (const xmlChar *)sys_cntr_name[i]));
          }
        }
        local_cpus      = sys_cntr[NUM_CPU];
        elapsed_seconds = sys_cntr[E_SEC] + sys_cntr[E_USEC]/1000000;
        result          = 0.0;
        xmit_rate       = 0.0;
        recv_rate       = 0.0;
        calibration     = sys_cntr[CALIBRATE];
        local_idle      = sys_cntr[IDLE] / elapsed_seconds;
        local_busy      = (sys_cntr[CALIBRATE]-local_idle)/sys_cntr[CALIBRATE];
        
        if (debug || loc_debug) {
          fprintf(where,"\tnum_cpus        = %f\n",local_cpus);
          fprintf(where,"\telapsed_seconds = %7.2f\n",elapsed_seconds);
          fprintf(where,"\tidle_cntr       = %e\n",sys_cntr[IDLE]);
          fprintf(where,"\tcalibrate_cntr  = %e\n",sys_cntr[CALIBRATE]);
          fprintf(where,"\tlocal_idle      = %e\n",local_idle);
          fprintf(where,"\tlocal_busy      = %e\n",local_busy);
          fflush(where);
        }
        if (stats_for_test == 0) {
          sys_stats_count++;
        }
        stats_for_test++;
        sys_seconds   += elapsed_seconds;
        sys_util      += local_busy;
      } /* end of processing system statistics */

      if(!xmlStrcmp(stats->name,(const xmlChar *)"test_stats")) {
        /* process test statistics */
        test_stats = 1;
        if (debug || loc_debug) {
          fprintf(where,"\tprocessing test_stats\n");
          fflush(where);
        }
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
          } else {
            test_cntr[i] = 0.0;
          }
          if (debug || loc_debug) {
            fprintf(where,"\t%12s test_cntr[%2d] = %10g\t'%s'\n",
                    cntr_name[i], i, test_cntr[i],
                    xmlGetProp(stats, (const xmlChar *)cntr_name[i]));
          }
        }
        elapsed_seconds = test_cntr[TST_E_SEC] + test_cntr[TST_E_USEC]/1000000;
        xmit_rate       = test_cntr[TST_X_BYTES]/(elapsed_seconds * 1000000);
        recv_rate       = test_cntr[TST_R_BYTES]/(elapsed_seconds * 1000000);
        xmit_trans_rate = test_cntr[TST_X_TRANS]/elapsed_seconds;
        recv_trans_rate = test_cntr[TST_R_TRANS]/elapsed_seconds;
        if (debug || loc_debug) {
          fprintf(where,"\txmit_rate = %7g\t%7g\n",
                  xmit_rate, test_cntr[TST_X_BYTES]);
          fprintf(where,"\trecv_rate = %7g\t%7g\n",
                  recv_rate, test_cntr[TST_R_BYTES]);
          fprintf(where,"\txmit_trans_rate = %7g\t%7g\n",
                  xmit_trans_rate, test_cntr[TST_X_TRANS]);
          fprintf(where,"\trecv_trans_rate = %7g\t%7g\n",
                  recv_trans_rate, test_cntr[TST_X_TRANS]);
          fflush(where);
        }
        if (sd_denominator == 0.0) {
          if (xmit_rate > 0.0 || recv_rate > 0.0) {
            sd_denominator = 1000000.0/(8.0*1024.0);
          } else {
            sd_denominator = 1.0;
          }
        }
        if (debug || loc_debug) {
          fprintf(where,"\tsd_denominator = %f\n",sd_denominator);
          fflush(where);
        }
        if (sd_denominator != 1.0) {
          result = recv_rate + xmit_rate;
        } else {
          result = recv_trans_rate + xmit_trans_rate;
        }
        if (debug || loc_debug) {
          fprintf(where,"\tresult    = %f\n",result);
          fflush(where);
        }
        if (stats_for_test == 0) {
          test_stats_count++;
        }
        stats_for_test++;

        /* accumulate results for the run */
        run_time      += elapsed_seconds;
        acc_results   += result;
        acc_xmit_rate += xmit_rate;
        acc_recv_rate += recv_rate;
      } /* other types of nodes just get skipped by this report routine */

      /* delete statistics entry from test */
      prev_stats = stats;
      stats = stats->next;
      xmlUnlinkNode(prev_stats);
      xmlFreeNode(prev_stats);
    } /* end of while to process all the statistics records for this test */

    /* should only have one stats record for this test otherwise error */
    if (stats_for_test > 1) {
      /* someone is playing games don't generate report*/
      fprintf(where,"More than one statistics measurement for test %d\n",
              stats_for_test);
      fprintf(where,"%s was not designed to deal with this.\n",
              "report_bsd_test_results");
      fprintf(where,"exiting netperf now!!\n");
      fflush(where);
      exit;
    }
          

    /* start printing bsd per statistics instance results */
    if ((flags && BSD_PER_TEST_FLAG) &&
        (last_hdr != BSD_PER_TEST_HDR)) {
      /* Display per test header */
      fprintf(outfd,"\n");
      fprintf(outfd,"INST ");
      fprintf(outfd,"TEST   ");
      fprintf(outfd,"TEST   ");
      if (sd_denominator != 1.0) {
        fprintf(outfd,"   DATA ");
        fprintf(outfd,"   XMIT ");
        fprintf(outfd,"   RECV ");
      } else {
        fprintf(outfd,"       TRANS ");
      }
      fprintf(outfd,"    CPU ");
      fprintf(outfd,"   CPU ");
#ifndef HPUX_1123
      fprintf(outfd,"   CPU ");
#else
      fprintf(outfd,"   CPU ");
      fprintf(outfd,"   CPU ");
      fprintf(outfd,"   CPU ");
#endif
      fprintf(outfd,"\n");
  
      fprintf(outfd,"NUM  ");
      fprintf(outfd,"Name   ");
      fprintf(outfd,"Time   ");
      if (sd_denominator != 1.0) {
        fprintf(outfd,"   RATE ");
        fprintf(outfd,"   RATE ");
        fprintf(outfd,"   RATE ");
      } else {
        fprintf(outfd,"      RATE ");
      }
      fprintf(outfd,"   CALI ");
      fprintf(outfd,"  IDLE ");
#ifndef HPUX_1123
      fprintf(outfd,"  BUSY ");
#else
      fprintf(outfd,"  USER ");
      fprintf(outfd,"   SYS ");
      fprintf(outfd,"  INTR ");
#endif
      fprintf(outfd,"\n");

      fprintf(outfd,"     ");
      fprintf(outfd,"       ");
      fprintf(outfd,"sec    ");
      if (sd_denominator != 1.0) {
        fprintf(outfd,"   mb/s ");
        fprintf(outfd,"   mb/s ");
        fprintf(outfd,"   mb/s ");
      } else {
        fprintf(outfd,"   trans/s ");
      }
      fprintf(outfd,"  cnt/s ");
      fprintf(outfd,"     %% ");
#ifndef HPUX_1123
      fprintf(outfd,"     %% ");
#else
      fprintf(outfd,"     %% ");
      fprintf(outfd,"     %% ");
      fprintf(outfd,"     %% ");
#endif
      fprintf(outfd,"\n");
      fflush(outfd);
      last_hdr = BSD_PER_TEST_HDR;
    }

    if (flags && BSD_PER_TEST_FLAG) {
      /* Display per test results */
      /* Display per run results */
      fprintf(outfd,"%-3d  ", count+1);				/*  0,5 */
      fprintf(outfd,"%-6s ",  test->id);			/*  5,7 */
      fprintf(outfd,"%-6.2f ",elapsed_seconds);		        /* 12,7 */
      if (test_stats) {
        if (sd_denominator != 1.0) {
          fprintf(outfd,"%7.2f ",result);			/* 19,8 */
          fprintf(outfd,"%7.2f ",xmit_rate);			/* 27,8 */
          fprintf(outfd,"%7.2f ",recv_rate);			/* 35,8 */
        } else {
          fprintf(outfd,"%10.2f ",result);			/* 19,11 */
        }
      } else {
        if (sd_denominator != 1.0) {
          fprintf(outfd,"%24s","");                             /* 19,24*/
        } else {
          fprintf(outfd,"%11s","");                             /* 19,11*/
        }
        fprintf(outfd,"%7.1e ",calibration);			/* 43,8 */
        fprintf(outfd,"%6.3f ",local_idle*100.0);		/* 51,7 */
#ifndef HPUX_1123
        fprintf(outfd,"%6.3f ",local_busy*100.0);		/* 58,7 */
#else
        fprintf(outfd,"%6.3f ",local_user*100.0);		/* 58,7 */
        fprintf(outfd,"%6.3f ",local_syst*100.0);		/* 65,7 */
        fprintf(outfd,"%6.3f ",local_intr*100.0);		/* 72,7 */
#endif
      }
      fprintf(outfd,"\n");					/* 79,1 */
    }
    /* end of printing bsd per test instance results */

    set_elt = set_elt->next;
  } /* end of while loop to collect per run results */

  /* should only have one system stats count otherwise error */
  if (sys_stats_count > 1) {
    /* someone is playing games don't generate report*/
    fprintf(where,"More than one system statistics measurement %d\n",
            sys_stats_count);
    fprintf(where,"%s was not designed to deal with this.\n",
            "report_bsd_test_results");
    fprintf(where,"exiting netperf now!!\n");
    fflush(where);
    exit;
  }
          
  run_time       = run_time / test_stats_count;
  service_demand = 1000000*local_cpus*sys_util/(acc_results*sd_denominator);

  /* save per run results for confidence calculations */
  my_data->results[count]      = acc_results;
  my_data->xmit_results[count] = acc_xmit_rate;
  my_data->recv_results[count] = acc_recv_rate;
  my_data->utilization[count]  = sys_util;
  my_data->servdemand[count]   = service_demand;
  my_data->run_time[count]     = run_time;
  count++;

  /* start printing bsd per run result summary */
  if ((flags && BSD_PER_RUN_FLAG) &&
      (last_hdr != BSD_PER_RUN_HDR)) {
    fprintf(outfd,"\n");
    /* Display per run header */
    fprintf(outfd,"INST ");
    fprintf(outfd,"SET    ");
    fprintf(outfd,"RUN    ");
    if (sd_denominator != 1.0) {
      fprintf(outfd,"   DATA ");
      fprintf(outfd,"   XMIT ");
      fprintf(outfd,"   RECV ");
    } else {
      fprintf(outfd,"     TRANS ");
    }
    fprintf(outfd,"     SD ");
    fprintf(outfd,"   CPU ");
    fprintf(outfd,"       ");
    fprintf(outfd,"       ");
    fprintf(outfd,"       ");
    fprintf(outfd,"\n");
  
    fprintf(outfd,"NUM  ");
    fprintf(outfd,"Name   ");
    fprintf(outfd,"Time   ");
    if (sd_denominator != 1.0) {
      fprintf(outfd,"   RATE ");
      fprintf(outfd,"   Rate ");
      fprintf(outfd,"   Rate ");
    } else {
      fprintf(outfd,"      RATE ");
    }
    fprintf(outfd,"   usec ");
    fprintf(outfd,"  Util ");
    fprintf(outfd,"       ");
    fprintf(outfd,"       ");
    fprintf(outfd,"       ");
    fprintf(outfd,"\n");

    fprintf(outfd,"     ");
    fprintf(outfd,"       ");
    fprintf(outfd,"sec    ");
    if (sd_denominator != 1.0) {
      fprintf(outfd,"   mb/s ");
      fprintf(outfd,"   mb/s ");
      fprintf(outfd,"   mb/s ");
    } else {
      fprintf(outfd,"   trans/s ");
    }
    fprintf(outfd,"    /KB ");
    fprintf(outfd," %%/100 ");
    fprintf(outfd,"       ");
    fprintf(outfd,"       ");
    fprintf(outfd,"       ");
    fprintf(outfd,"\n");
    fflush(outfd);
    last_hdr = BSD_PER_RUN_HDR;
  }

  if (flags && BSD_PER_RUN_FLAG) {
    /* Display per run results */
    fprintf(outfd,"%-3d  ", count);				/*  0,5 */
    fprintf(outfd,"%-6s ",  test_set->id);			/*  5,7 */
    fprintf(outfd,"%-6.2f ",run_time);			        /* 12,7 */
    if (sd_denominator != 1.0) {
      fprintf(outfd,"%7.2f ",acc_results);			/* 19,8 */
      fprintf(outfd,"%7.2f ",acc_xmit_rate);			/* 27,8 */
      fprintf(outfd,"%7.2f ",acc_recv_rate);			/* 35,8 */
    } else {
      fprintf(outfd,"%10.2f ",acc_results);			/* 19,11*/
    }
    fprintf(outfd,"%7.3f ",service_demand);			/* 43,8 */
    fprintf(outfd,"%6.4f ",sys_util);				/* 51,7 */
#ifdef OFF
    fprintf(outfd,"%6.4f ",something to be added later);	/* 58,7 */
    fprintf(outfd,"%6.4f ",something to be added later);	/* 65,7 */
    fprintf(outfd,"%6.4f ",something to be added later);	/* 72,7 */
#endif
    fprintf(outfd,"\n");					/* 79,1 */
  }
  /* end of printing bsd per run result summary */
  /* start printing bsd per run result summary */
 
  /* calculate confidence and summary result values */
  elapsed_seconds        = get_confidence(my_data->run_time,
                                          &(test_set->confidence),
                                          &ave_time);
  result_confidence      = get_confidence(my_data->results,
                                          &(test_set->confidence),
                                          &result_measured_mean);
  cpu_util_confidence    = get_confidence(my_data->utilization,
                                          &(test_set->confidence),
                                          &cpu_util_measured_mean);
  serv_demand_confidence = get_confidence(my_data->servdemand,
                                          &(test_set->confidence),
                                          &service_demand_measured_mean);
  xmit_confidence        = get_confidence(my_data->xmit_results,
                                          &(test_set->confidence),
                                          &xmit_measured_mean);
  recv_confidence        = get_confidence(my_data->recv_results,
                                          &(test_set->confidence),
                                          &recv_measured_mean);
  if (result_confidence > cpu_util_confidence) {
    if (cpu_util_confidence > serv_demand_confidence) {
      confidence  = serv_demand_confidence;
    } else {
      confidence  = cpu_util_confidence;
    }
  } else {
    if (result_confidence > serv_demand_confidence) {
      confidence  = serv_demand_confidence;
    } else {
      confidence  = result_confidence;
    }
  }
  test_set->confidence.value = confidence;

  /* start printing bsd test result summary */
  if (last_hdr != BSD_SUMMARY_HDR) {
    /* Display summary header */
    fprintf(outfd,"\n");
    fprintf(outfd,"AVE  ");
    fprintf(outfd,"SET    ");
    fprintf(outfd,"TEST   ");
    if (sd_denominator != 1.0) {
      fprintf(outfd,"   DATA ");
    } else {
      fprintf(outfd,"  TRANS ");
    }
    fprintf(outfd,"   conf ");
    fprintf(outfd,"    Min ");
    fprintf(outfd,"    Max ");
    fprintf(outfd,"   CPU ");
    fprintf(outfd,"   +/- ");
    fprintf(outfd,"    SD ");
    fprintf(outfd,"   +/- ");
    fprintf(outfd,"\n");
  
    fprintf(outfd,"Over ");
    fprintf(outfd,"Name   ");
    fprintf(outfd,"Time   ");
    if (sd_denominator != 1.0) {
      fprintf(outfd,"   RATE ");
    } else {
      fprintf(outfd,"   RATE ");
    }
    fprintf(outfd,"    +/- ");
    fprintf(outfd,"   Rate ");
    fprintf(outfd,"   Rate ");
    fprintf(outfd,"  Util ");
    fprintf(outfd,"  Util ");
    fprintf(outfd,"  usec ");
    fprintf(outfd,"  usec ");
    fprintf(outfd,"\n");

    fprintf(outfd,"Num  ");
    fprintf(outfd,"       ");
    fprintf(outfd,"sec    ");
    if (sd_denominator != 1.0) {
      fprintf(outfd,"   mb/s ");
      fprintf(outfd,"   mb/s ");
      fprintf(outfd,"   mb/s ");
      fprintf(outfd,"   mb/s ");
    } else {
      fprintf(outfd," tran/s ");
      fprintf(outfd," tran/s ");
      fprintf(outfd," tran/s ");
      fprintf(outfd," tran/s ");
    }
    fprintf(outfd," %%/100 ");
    fprintf(outfd," %%/100 ");
    fprintf(outfd,"   /KB ");
    fprintf(outfd,"   /KB ");
    fprintf(outfd,"\n");
    fflush(outfd);
    last_hdr = BSD_SUMMARY_HDR;
  }

  /* always print summary results at end of last call through loop */
  if ((count == max_count) || ((confidence >= 0) && (count >= min_count))) {
    double result_minimum = MAXDOUBLE;
    double result_maximum = MINDOUBLE;
    for (i = 0; i < count; i++) {
      if (result_minimum > my_data->results[i]) {
        result_minimum = my_data->results[i];
      }
      if (result_maximum < my_data->results[i]) {
        result_maximum = my_data->results[i];
      }
    }
    fprintf(outfd,"A%-3d ",count);				/*  0,5 */
    fprintf(outfd,"%-6s ",test_set->id);			/*  5,7 */
    fprintf(outfd,"%-6.2f ",ave_time);			        /* 12,7 */
    if (sd_denominator != 1.0) {
      fprintf(outfd,"%7.2f ",result_measured_mean);		/* 19,8 */
      fprintf(outfd,"%7.3f ",result_confidence);		/* 27,8 */
      fprintf(outfd,"%7.2f ",result_minimum);			/* 35,8 */
      fprintf(outfd,"%7.2f ",result_maximum);			/* 43,8 */
    } else {
      fprintf(outfd,"%7.0f ",result_measured_mean);		/* 19,8 */
      fprintf(outfd,"%7.2f ",result_confidence);		/* 27,8 */
      fprintf(outfd,"%7.0f ",result_minimum);			/* 35,8 */
      fprintf(outfd,"%7.0f ",result_maximum);			/* 43,8 */
    }
    fprintf(outfd,"%6.4f ",cpu_util_measured_mean);		/* 51,7 */
    fprintf(outfd,"%6.4f ",cpu_util_confidence);		/* 58,7 */
    fprintf(outfd,"%6.3f ",service_demand_measured_mean);	/* 65,7 */
    fprintf(outfd,"%6.3f ",serv_demand_confidence);		/* 72,7 */
    fprintf(outfd,"\n");					/* 79,1 */
    fflush(outfd);
    free(my_data);
  }
  /* end of printing bsd test result summary */
  /* start printing bsd test result summary */
  /* end of printing bsd test result summary */

} /* end of report_bsd_test_results */

