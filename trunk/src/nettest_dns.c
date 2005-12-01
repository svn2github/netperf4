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
char    nettest_dns_id[]="\
@(#)nettest_dns.c (c) Copyright 2005 Hewlett-Packard Co. $Id$";

#ifdef HAVE_CONFIG_H
#include "config.h"
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
/*      nettest_dns.c                                           */
/*                                                              */
/*      the actual test routines...                             */
/*                                                              */
/*      send_dns_rr()           perform a DNS req/rsp test      */
/*                                                              */
/****************************************************************/



#include <stdio.h>
#include <values.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <errno.h>

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif

#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include "netperf.h"

#ifdef HISTOGRAM
#include "hist.h"
#else
#define HIST  void*
#endif /* HISTOGRAM */

#include "nettest_dns.h"

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
      } else {
        sprintf(error_msg,"bad state transition from %s state",state_name);
        report_test_failure( test,
                             "set_test_state",
                             DNSE_REQUESTED_STATE_INVALID,
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
      free(test->test_specific_data);
      test->test_specific_data = NULL;
      test->new_state = TEST_DEAD;
    }
  }
}



static void
get_dependency_data(test_t *test, int type, int protocol)
{
  dns_data_t *my_data = test->test_specific_data;

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

  if ((-1 == type) &&
      (-1 == protocol)) {
    /* that means we retrieve type and protocol based on dependency
       data if there is a use_tcp attribute _and_ it says yes or true,
       then we will use TCP. otherwise we will use UDP. raj 2005-11-18
      */
    string = xmlGetProp(test->dependency_data,(const xmlChar *)"use_tcp");
    if (NULL == string) {
      hints.ai_socktype = SOCK_DGRAM;
      hints.ai_protocol = IPPROTO_UDP;
      my_data->use_tcp = 0;
    }
    else {
      if (!strcasecmp((char *)string,"true") ||
	  !strcasecmp((char *)string,"yes")) {
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	my_data->use_tcp = 1;
      }
      else {
      hints.ai_socktype = SOCK_DGRAM;
      hints.ai_protocol = IPPROTO_UDP;
      my_data->use_tcp = 0;
      }	
    }
  }
  else {
  }
  hints.ai_flags    = AI_ADDRCONFIG;

  /* we depend on the parser making sure these were actually
     present... */
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
  } else {
    if (test->debug) {
      fprintf(test->where,"get_dependency_data: getaddrinfo returned %d %s\n",
              error, gai_strerror(error));
      fflush(test->where);
    }
    report_test_failure(test,
                        "get_dependency_data",
                        DNSE_GETADDRINFO_ERROR,
                        gai_strerror(error));
  }
}


static void
set_dependent_data(test)
  test_t *test;
{
  
  dns_data_t  *my_data = test->test_specific_data;
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
                          DNSE_XMLSETPROP_ERROR,
                          "error setting properties for dependency data");
    }
  } else {
    report_test_failure(test,
                        "set_dependent_data",
                        DNSE_XMLNEWNODE_ERROR,
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
  dns_data_t  *my_data = test->test_specific_data;

  int family           = my_data->locaddr->ai_family;
  int type             = my_data->locaddr->ai_socktype;
  int lss_size         = my_data->send_buf_size;
  int lsr_size         = my_data->recv_buf_size;

  int temp_socket;
  int one;
  int sock_opt_len;

  if (test->debug) {
    fprintf(test->where,
            "create_data_socket: calling socket family = %d type = %d\n",
            family, type);
    fflush(test->where);
  }
  /*set up the data socket                        */
  temp_socket = socket(family,
                       type,
                       0);

  if (CHECK_FOR_INVALID_SOCKET) {
    report_test_failure(test,
                        "create_data_socket",
                        DNSE_SOCKET_ERROR,
                        "error creating socket");
    return(temp_socket);
  }

  if (test->debug) {
    fprintf(test->where,
            "create_data_socket: socket %d obtained...\n",
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
                  &lss_size, sizeof(int)) < 0) {
      report_test_failure(test,
                          "create_data_socket",
                          DNSE_SETSOCKOPT_ERROR,
                          "error setting local send socket buffer size");
      return(temp_socket);
    }
    if (test->debug > 1) {
      fprintf(test->where,
              "nettest_dns: create_data_socket: SO_SNDBUF of %d requested.\n",
              lss_size);
      fflush(test->where);
    }
  }

  if (lsr_size > 0) {
    if(setsockopt(temp_socket, SOL_SOCKET, SO_RCVBUF,
                  &lsr_size, sizeof(int)) < 0) {
      report_test_failure(test,
                          "create_data_socket",
                          DNSE_SETSOCKOPT_ERROR,
                          "error setting local recv socket buffer size");
      return(temp_socket);
    }
    if (test->debug > 1) {
      fprintf(test->where,
              "nettest_dns: create_data_socket: SO_RCVBUF of %d requested.\n",
              lsr_size);
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
        "nettest_dns: create_data_socket: getsockopt SO_SNDBUF: errno %d\n",
        errno);
    fflush(test->where);
    lss_size = -1;
  }
  if (getsockopt(temp_socket,
                 SOL_SOCKET,
                 SO_RCVBUF,
                 (char *)&lsr_size,
                 &sock_opt_len) < 0) {
    fprintf(test->where,
        "nettest_dns: create_data_socket: getsockopt SO_RCVBUF: errno %d\n",
        errno);
    fflush(test->where);
    lsr_size = -1;
  }
  if (test->debug) {
    fprintf(test->where,
            "nettest_dns: create_data_socket: socket sizes determined...\n");
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
              "netperf: create_data_socket: nodelay: errno %d\n",
              errno);
      fflush(test->where);
    }

    if (test->debug > 1) {
      fprintf(test->where,
              "netperf: create_data_socket: TCP_NODELAY requested...\n");
      fflush(test->where);
    }
  }
#else /* TCP_NODELAY */

  my_data->no_delay = 0;

#endif /* TCP_NODELAY */

  return(temp_socket);

}


static void
test_specific_data_init(dns_data_t *new) {

  struct timeval foo;

  /* first, zero the thing entirely */
  memset(new,0,sizeof(dns_data_t));

  /* now, set some specific fields.  some of these may be redundant */
  new->request_source = NULL;

  new->query_socket = -1;

  /* might be good to randomize the initial request_id.  in the
     overall scheme of things it may not really matter, but if we are
     staring at a tcpdump trace of many threads sending
     requests... and knuth only knows what the DNS server might do
     with the request_id if we have all the threads starting with the
     same one...  raj 2005-11-17 */
  gettimeofday(&foo,NULL);
  srandom(foo.tv_usec);
  new->request_id = random() % UINT16_MAX;

}

/* at present we only support Internet class */
static int
strtoclass(char class_string[]){
  if (!strcasecmp(class_string,"C_IN")) return(C_IN);
  else return(C_NONE);
}

static int
strtotype(char type_string[]){
  if (!strcasecmp(type_string,"T_A")) return(T_A);
  else if (!strcasecmp(type_string,"T_NS")) return(T_NS);
  else if (!strcasecmp(type_string,"T_MD")) return(T_MD);
  else if (!strcasecmp(type_string,"T_MF")) return(T_MF);
  else if (!strcasecmp(type_string,"T_CNAME")) return(T_CNAME);
  else if (!strcasecmp(type_string,"T_SOA")) return(T_SOA);
  else if (!strcasecmp(type_string,"T_MB")) return(T_MB);
  else if (!strcasecmp(type_string,"T_MG")) return(T_MG);
  else if (!strcasecmp(type_string,"T_MR")) return(T_MR);
  else if (!strcasecmp(type_string,"T_NULL")) return(T_NULL);
  else if (!strcasecmp(type_string,"T_WKS")) return(T_WKS);
  else if (!strcasecmp(type_string,"T_PTR")) return(T_PTR);
  else if (!strcasecmp(type_string,"T_HINFO")) return(T_HINFO);
  else if (!strcasecmp(type_string,"T_MINFO")) return(T_MINFO);
  else if (!strcasecmp(type_string,"T_MX")) return(T_MX);
  else if (!strcasecmp(type_string,"T_TXT")) return(T_TXT);
  else if (!strcasecmp(type_string,"T_AFSDB")) return(T_AFSDB);
  else if (!strcasecmp(type_string,"T_X25")) return(T_X25);
  else if (!strcasecmp(type_string,"T_ISDN")) return(T_ISDN);
  else if (!strcasecmp(type_string,"T_RT")) return(T_RT);
  else if (!strcasecmp(type_string,"T_NSAP")) return(T_NSAP);
  else if (!strcasecmp(type_string,"T_NSAP_PTR")) return(T_NSAP_PTR);
  else if (!strcasecmp(type_string,"T_ATMA")) return(T_ATMA);
  else if (!strcasecmp(type_string,"T_NAPTR")) return(T_NAPTR);
#ifdef T_A6
  else if (!strcasecmp(type_string,"T_A6")) return(T_A6);
#endif
#ifdef T_AAAA
  else if (!strcasecmp(type_string,"T_AAAA")) return(T_AAAA);
#endif
#ifdef T_DNAME
  else if (!strcasecmp(type_string,"T_DNAME")) return(T_DNAME);
#endif
  else if (!strcasecmp(type_string,"T_AXFR")) return(T_AXFR);
  else if (!strcasecmp(type_string,"T_MAILB")) return(T_MAILB);
  else if (!strcasecmp(type_string,"T_MAILA")) return(T_MAILA);
  else if (!strcasecmp(type_string,"T_ANY")) return(T_ANY);
  else return(T_NULL);
}

/* read the next line from the query_source stream, and build the
   query to send.  */
static int
get_next_dns_request(test_t *test, char *request_buffer, int buflen) {

  dns_data_t *my_data = GET_TEST_DATA(test);

  char temp_name[NS_MAXDNAME]; /* from arpa/nameser.h */
  char temp_class[64];   /* 64 seems like a bit much.  and by rights
			    we probably should be doing stuff to make
			    sure we don't overflow these things on the
			    fscanf... */
  char temp_type[64];
  int temp_success;

  if (my_data->request_source) {
    /* ah, good, we do have a file */
    if (fscanf(my_data->request_source,
	       "%s %s %s %d\n",
	       temp_name,
	       temp_class,
	       temp_type,
	       &temp_success) == EOF) {
      rewind(my_data->request_source);
      if (fscanf(my_data->request_source,
		 "%s %s %s %d\n",
		 temp_name,
		 temp_class,
		 temp_type,
		 &temp_success) == EOF) {
	/* something like that should never happen */
	return(-1);
      }
    }
    return(res_mkquery(QUERY,
		       temp_name,
		       strtoclass(temp_class),
		       strtotype(temp_type),
		       NULL,
		       0,
		       NULL,
		       request_buffer,
		       buflen));
  }
  else {
    /* well, now what should we do? */
    return(-1);
  }
}

static void
pick_random_start(test_t *test) {
  dns_data_t *my_data;
  char temp_name[NS_MAXDNAME]; /* from arpa/nameser.h */
  char temp_class[64];   /* 64 seems like a bit much.  and by rights
			    we probably should be doing stuff to make
			    sure we don't overflow these things on the
			    fscanf... */
  char temp_type[64];
  int  temp_success;
  
  struct timeval foo;
  int i;
  
  my_data = GET_TEST_DATA(test);
  gettimeofday(&foo,NULL);
  srand(foo.tv_usec);
  i = rand() % 65536;
  for (;i>0;i--) {
    if (my_data->request_source) {
      /* ah, good, we do have a file */
      if (fscanf(my_data->request_source,
		 "%s %s %s %d\n",
		 temp_name,
		 temp_class,
		 temp_type,
		 &temp_success) == EOF) {
	rewind(my_data->request_source);
      }
    }
  }
}


dns_data_t *
dns_test_init(test_t *test, int type, int protocol)
{
  dns_data_t *new_data;
  xmlNodePtr  args;
  xmlNodePtr  socket_args = NULL;
  xmlNodePtr  dns_args = NULL;
  xmlChar    *string;
  xmlChar    *units;
  xmlChar    *localhost;
  xmlChar    *localport;
  int         localfam;

  int               count;
  int               error;
  struct addrinfo   hints;
  struct addrinfo  *local_ai;
  struct addrinfo  *local_temp;

  /* allocate memory to store the information about this test */
  new_data = (dns_data_t *)malloc(sizeof(dns_data_t));

  args = test->node->xmlChildrenNode;
  /* make sure we go though everything, and do not assume anything
     about the order of the elements. raj 2005-11-30 */
  while (args != NULL) {
    if (!xmlStrcmp(args->name,(const xmlChar *)"dependency_data")) {
      test->dependency_data = args;
    }
    else if (!xmlStrcmp(args->name,(const xmlChar *)"dns_args")) {
      dns_args = args;
    }
    else if (!xmlStrcmp(args->name,(const xmlChar *)"socket_args")) {
      socket_args = args;
    } 
    args = args->next;
  }
  
  /* probably a good idea to make sure that new_data is real */
  if ((socket_args != NULL) &&
      (NULL != new_data)) {
    /* zero the dns test specific data structure */
    test_specific_data_init(new_data);

    /* while we are not using it as a means to fill buffers with data,
       we do use "fill_file" as the way to tell where the list of
       dns_requests happen to be */
    string =  xmlGetProp(socket_args,(const xmlChar *)"fill_file");
    /* fopen the fill file it will be used when finding work to do */
    if (string) {
      new_data->request_source = fopen((char *)string,"r");
      pick_random_start(test);
    }

    /* we are relying on the good graces of the validating and
       attribute filling of libxml when we parsed the message that got
       us here... */
    string =  xmlGetProp(socket_args,(const xmlChar *)"send_buffer_size");
    units  =  xmlGetProp(socket_args,(const xmlChar *)"send_buffer_units");
    new_data->send_buf_size = convert(string,units);

    string =  xmlGetProp(socket_args,(const xmlChar *)"recv_buffer_size");
    units  =  xmlGetProp(socket_args,(const xmlChar *)"recv_buffer_units");
    new_data->recv_buf_size = convert(string,units);

    /* we need to add code somewhere here to get stuff such as whether
       we should use TCP, set TCP_NODELAY, keep the TCP connection
       open... */

    /* now get and initialize the local addressing info */
    string   =  xmlGetProp(socket_args,(const xmlChar *)"family");
    localfam = strtofam(string);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = localfam;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    hints.ai_flags    = 0;

    localhost = xmlGetProp(socket_args,(const xmlChar *)"local_host"),
    localport = xmlGetProp(socket_args,(const xmlChar *)"local_service"),
    count = 0;
    do {
      error = getaddrinfo( (char *)localhost, (char *)localport,
                              &hints, &local_ai);
      count += 1;
      if (error == EAI_AGAIN) {
        if (test->debug) {
          fprintf(test->where,
		  "%s sleeping on getaddrinfo EAI_AGAIN\n",
		  (char *)__func__);
          fflush(test->where);
        }
        sleep(1);
      }
    } while ((error == EAI_AGAIN) && (count <= 5));
    
    if (test->debug) {
      dump_addrinfo(test->where, local_ai, localhost, localport, localfam);
    }

    if (!error) {
      /* OK, so we only do this once, but shouldn't we copy rather
	 than assign here so we can do a freeaddrinfo()?  raj
	 2005-11-17 */
      new_data->locaddr = local_ai;
    } 
    else {
      if (test->debug) {
        fprintf(test->where,
		"%s: getaddrinfo returned %d %s\n",
		(char *)__func__,
                error, 
		gai_strerror(error));
        fflush(test->where);
      }
      report_test_failure(test,
                          (char *)__func__,
                          DNSE_GETADDRINFO_ERROR,
                          gai_strerror(error));
    }
    if (dns_args) {
      string =  xmlGetProp(dns_args,(const xmlChar *)"max_outstanding");
      if (string) {
	new_data->max_outstanding = atoi((char *)string);
      }
      else {
	new_data->max_outstanding = 1;
      }
      string =  xmlGetProp(dns_args,(const xmlChar *)"timeout");
      if (string) {
	new_data->timeout = atoi((char *)string);
      }
      else {
	new_data->timeout = 5000;
      }
    }
    else {
      /* there was no dns_args element in the test element, so use the
	 program defaults. raj 2005-11-29 */
      new_data->max_outstanding = 1;
      new_data->timeout = 5000;
    }
  }
  else {
    report_test_failure(test,
                        (char *) __func__,
                        DNSE_NO_SOCKET_ARGS,
                        "no socket_arg element was found");
  }

  test->test_specific_data = new_data;
  return(new_data);
}

static void
update_elapsed_time(dns_data_t *my_data)
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
dns_test_clear_stats(dns_data_t *my_data)
{
  int i;
  for (i = 0; i < DNS_MAX_COUNTERS; i++) {
    my_data->stats.counter[i] = 0;
  }
  my_data->elapsed_time.tv_usec = 0;
  my_data->elapsed_time.tv_sec  = 0;
  gettimeofday(&(my_data->prev_time),NULL);
  my_data->curr_time = my_data->prev_time;
  return(NPE_SUCCESS);
}

void
dns_test_decode_stats(xmlNodePtr stats, test_t *test)
{
  if (test->debug) {
    fprintf(test->where,"dns_test_decode_stats: entered for %s test %s\n",
            test->id, test->test_name);
    fflush(test->where);
  }
}

static xmlNodePtr
dns_test_get_stats(test_t *test)
{
  xmlNodePtr  stats = NULL;
  xmlAttrPtr  ap    = NULL;
  int         i,j;
  char        value[32];
  char        name[32];
  uint64_t    loc_cnt[DNS_MAX_COUNTERS];

  dns_data_t *my_data = GET_TEST_DATA(test);

  if (test->debug) {
    fprintf(test->where,"dns_test_get_stats: entered for %s test %s\n",
            test->id, test->test_name);
    fflush(test->where);
  }
  if ((stats = xmlNewNode(NULL,(xmlChar *)"test_stats")) != NULL) {
    /* set the properites of the test_stats message -
       the tid and time stamps/values and counter values  sgb 2004-07-07 */

    ap = xmlSetProp(stats,(xmlChar *)"tid",test->id);
    for (i = 0; i < DNS_MAX_COUNTERS; i++) {
      loc_cnt[i] = my_data->stats.counter[i];
      if (test->debug) {
        fprintf(test->where,"DNS_COUNTER%X = %#llx\n",i,loc_cnt[i]);
      } 
    }
    if (GET_TEST_STATE == TEST_MEASURE) {
      gettimeofday(&(my_data->curr_time), NULL);
      if (ap != NULL) {
        sprintf(value,"%#ld",my_data->curr_time.tv_sec);
        ap = xmlSetProp(stats,(xmlChar *)"time_sec",(xmlChar *)value);
        if (test->debug) {
          fprintf(test->where,"time_sec=%s\n",value);
          fflush(test->where);
        }
      }
      if (ap != NULL) {
        sprintf(value,"%#ld",my_data->curr_time.tv_usec);
        ap = xmlSetProp(stats,(xmlChar *)"time_usec",(xmlChar *)value);
        if (test->debug) {
          fprintf(test->where,"time_usec=%s\n",value);
          fflush(test->where);
        }
      }
    } else {
      if (ap != NULL) {
        sprintf(value,"%#ld",my_data->elapsed_time.tv_sec);
        ap = xmlSetProp(stats,(xmlChar *)"elapsed_sec",(xmlChar *)value);
        if (test->debug) {
          fprintf(test->where,"elapsed_sec=%s\n",value);
          fflush(test->where);
        }
      }
      if (ap != NULL) {
        sprintf(value,"%#ld",my_data->elapsed_time.tv_usec);
        ap = xmlSetProp(stats,(xmlChar *)"elapsed_usec",(xmlChar *)value);
        if (test->debug) {
          fprintf(test->where,"elapsed_usec=%s\n",value);
          fflush(test->where);
        }
      }
    }
    for (i = 0; i < DNS_MAX_COUNTERS; i++) {
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
} /* end of dns_test_get_stats */



static void
send_dns_rr_preinit(test_t *test)
{
  dns_data_t       *my_data;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  my_data   = test->test_specific_data;

  /* let get_dependency_data know to pick type and protocol based on
     the dependency info */
  get_dependency_data(test, -1, -1);
  my_data->query_socket = create_data_socket(test);
}

static uint32_t
send_dns_rr_init(test_t *test)
{
  dns_data_t       *my_data;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  my_data   = test->test_specific_data;

  if (test->debug) {
    fprintf(test->where,"%s: in INIT state making connect call\n",__func__);
    fflush(test->where);
  }
  if (connect(my_data->query_socket,
              my_data->remaddr->ai_addr,
              my_data->remaddr->ai_addrlen) < 0) {
    report_test_failure(test,
                        __func__,
                        DNSE_CONNECT_FAILED,
                        "data socket connect failed");
  } else {
    if (test->debug) {
      fprintf(test->where,"%s: connected and moving to IDLE\n",__func__);
      fflush(test->where);
    }
  }
  return(TEST_IDLE);
}

static void
send_dns_rr_idle_link(test_t *test, int last_len)
{
  int               len;
  uint32_t          new_state;
  dns_data_t       *my_data;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  my_data   = test->test_specific_data;
  len       = last_len;

  new_state = CHECK_REQ_STATE;
  while (new_state == TEST_LOADED) {
    sleep(1);
    new_state = CHECK_REQ_STATE;
  }
  if (new_state == TEST_IDLE) {
    if (test->debug) {
      fprintf(test->where,"%s: transition from LOAD to IDLE\n",__func__);
      fflush(test->where);
    }
    /* it may not be pretty, but it is sufficient and effective for
       our nefarious porpoises here - just close the socket,
       regardless of flavor, as we don't care one whit about flushing,
       and are not going to worry about attempts to reuse a TCP
       connection in TIME_WAIT when TCP connections are being used. */
    if (close(my_data->query_socket) == -1) {
      report_test_failure(test,
			  __func__,
                          DNSE_SOCKET_SHUTDOWN_FAILED,
                          "failure shuting down data socket");
    } 
    else {
      my_data->query_socket = -1;
    }
  } else {
    /* a transition to a state other than TEST_IDLE was requested
       after the link was closed in the TEST_LOADED state */
    report_test_failure(test,
			__func__,
                        DNSE_DATA_CONNECTION_CLOSED_ERROR,
                        "data connection closed and non idle state requested");

  }
}


/* return the the next available free status entry.  if none exist,
   return NULL. raj 2005-11-30. question - should we also be checking
   for requests which have timed-out? why not...it might be the way we
   get a free entry. however, we won't scan the entire thing just to
   check all the  */

static dns_request_status_t *
get_free_status_entry(test_t *test) {
  dns_request_status_t *temp;
  dns_request_status_t *free;
  int                   count;
  dns_data_t           *my_data;
  NETPERF_TIMESTAMP_T   now;

  free = NULL;
  my_data = GET_TEST_DATA(test);
  netperf_timestamp(&now);

  /* we ass-u-me that max_outstanding is never so large that just a
     simplistic linear search is heinous.  famous last words :) raj
     2005-11-30 */ 
  for (count = 0; 
       (NULL == free) && (count < my_data->max_outstanding);
       count++) {
    temp = &(my_data->outstanding_requests[count]);
    if (temp->active) {
      if (delta_milli(&(temp->sent_time),&now) >= my_data->timeout) {
	if (test->debug) {
	  fprintf(test->where,
		  "EXPIRING message %d in %s after %d ms\n",
		  temp->id,
		  __func__,
		  delta_milli(&(temp->sent_time),&now));
	  fflush(test->where);
	}
	memset(temp,0,sizeof(dns_request_status_t));
	my_data->num_outstanding--;
	free = temp;
      }
    }
    else {
      free = temp;
    }
  }
  return(free);
}

/* find a status entry matching the specified message_id.  while we
   are cruising through the list, we migh as well look for expired
   entries. raj 2005-11-30 */

static dns_request_status_t *
find_status_entry(test_t *test, uint16_t message_id) {
  dns_request_status_t *temp;
  dns_request_status_t *match;
  int                   count;
  dns_data_t           *my_data;
  NETPERF_TIMESTAMP_T   now;

  match = NULL;
  my_data = GET_TEST_DATA(test);
  netperf_timestamp(&now);

  /* we ass-u-me that max_outstanding is never so large that just a
     simplistic linear search is heinous.  famous last words :) raj
     2005-11-30 */ 
  for (count = 0; 
       (NULL == match) && (count < my_data->max_outstanding);
       count++) {
    temp = &(my_data->outstanding_requests[count]);
    if (temp->active) {
      /* it is an active entry, is it a match ?*/
      if (temp->id == message_id) {
	/* we match, so no matter what we will decrement the count of
	   outstanding requests, but is it expired? */
	if (delta_milli(&(temp->sent_time),&now) >= my_data->timeout){
	  printf("EXPIRING message %d in %s\n",temp->id, __func__);
	  memset(temp,0,sizeof(dns_request_status_t));
	}
	else {
	  match = temp;
	}
	my_data->num_outstanding--;
      }
      else {
	/* didn't match, but is it expired, be sure to watch the sense
	   of the comparison */
	if (delta_milli(&(temp->sent_time),&now) >= my_data->timeout){
	  if (test->debug) {
	    fprintf(test->where,
		    "EXPIRING message %d from %s after %d ms\n",
		    temp->id,
		    __func__,
		    delta_milli(&(temp->sent_time),&now));
	    fflush(test->where);
	  }
	  memset(temp,0,sizeof(dns_request_status_t));
	  my_data->num_outstanding--;
	}
      }
    }
  }
  return(match);
}

/* walk the list of entries, looking for those that have expired */
static void
scan_for_expired_requests(test_t *test) {
  dns_request_status_t *temp;
  int                   count;
  dns_data_t           *my_data;
  NETPERF_TIMESTAMP_T   now;

  my_data = GET_TEST_DATA(test);
  netperf_timestamp(&now);

  /* we ass-u-me that max_outstanding is never so large that just a
     simplistic linear search is heinous.  famous last words :) raj
     2005-11-30 */ 
  for (count = 0; 
       (count < my_data->max_outstanding);
       count++) {
    temp = &(my_data->outstanding_requests[count]);
    if ((temp->active) && 
	(delta_milli(&(temp->sent_time),&now) >= my_data->timeout)) {
      if (test->debug) {
	fprintf(test->where,
		"EXPIRING message %d from %s after %d ms\n",
		temp->id,
		__func__,
		delta_milli(&(temp->sent_time),&now));
	fflush(test->where);
      }
      memset(temp,0,sizeof(dns_request_status_t));
      my_data->num_outstanding--;
    }
  }
}

/* send at most max_outstanding requests onto the network, return
   errors as appropriate - either -1 if something went wrong, or the
   number of requests sent, which could be zero.  if we ever go
   non-blocking on the socket, we may need to be a bit more
   sophisticated in our error checking and send calls - especially if
   we ever start using TCP. raj 2005-11-30 */
static int
send_dns_requests(test_t *test) 
{
  dns_data_t  *my_data;
  int          sent = 0;
  int          req_size;
  int          len;
  uint16_t     request_buffer[NS_PACKETSZ/sizeof(uint16_t)];  
  uint16_t     message_id;

  dns_request_status_t *status_entry;

  my_data = GET_TEST_DATA(test);

  while (my_data->num_outstanding < my_data->max_outstanding ) {
    if (test->debug) {
      fprintf(test->where,"MAX %d OUTSTANDING %d in %s\n",
	      my_data->max_outstanding,
	      my_data->num_outstanding,
	      (char *)__func__);
      fflush(test->where);
    }
    /* go through and build the next request to send */
    req_size = get_next_dns_request(test, 
				    (char *)request_buffer, 
				    sizeof(request_buffer));
      
    /* set the ID as apropriate since it is a uint16_t, we'll not
       worry about "overflow" as it will just be dealt with
       "naturally. we use the magic number of "0" based on "knowing"
       that the ID is the first thing in the message, and it seems
       there is no commonly defined structure for a DNS reqeust
       header?  raj 2005-11-18 */
      
    message_id = request_buffer[0] = my_data->request_id++; 

    if (test->debug) {
      fprintf(test->where,
	      "SENDING message %d\n",
	      message_id);
      fflush(test->where);
    }
    /* now stash some state away so we can deal with the response */
    status_entry = get_free_status_entry(test);
    if (status_entry) {
      status_entry->active = 1;
      status_entry->success = 1;
      netperf_timestamp(&(status_entry->sent_time));
      status_entry->id = message_id;
    }
    else {
      /* this could be bad?  or is it just an expired entry? */
      printf("Hey dummy, there were no free status entries!\n");
      return(-1);
    }
    /* send data for the test. we can use send() rather than
       sendto() because in _init we will have called connect() on
       the socket.  now, if we are using UDP and the server happens
       to reply from a source IP address other than the one to which
       we have connected... well...  raj 2005-11-18 */
      
    /* if we are ever going to _really_ pace these things, we need
       logic to decide if it is time to send another request or
       not */
      
    if((len=send(my_data->query_socket,
		 request_buffer,
		 req_size,
		 0)) != req_size) {
      /* this macro hides Windows differences */
      if (CHECK_FOR_SEND_ERROR(len)) {
	report_test_failure(test,
			    (char *)__func__,
			    DNSE_DATA_SEND_ERROR,
			    "data send error");
	/* do we need to do something additional here? probably */
	return(-1);
      }
    }
    /* my_data->stats.named.queries_sent++; */
    /* my_data->stats.named.query_bytes_sent += len; */
    my_data->num_outstanding++;
    sent++;
  }
  return(sent);
}

static int
recv_dns_responses(test_t *test)
{
}

static uint32_t
send_dns_rr_meas(test_t *test)
{
  uint32_t          new_state;
  int               len;
  int               ret;
  int               i;
  int               response_len;
  int               bytes_left;
  int               req_size;
  uint16_t         *rsp_ptr;
  uint16_t          message_id;
  uint16_t          response_id;
  int               num_to_check;
  dns_data_t       *my_data;
  dns_request_status_t *status_entry;
  struct pollfd     fds;
  int               keep_going=1;
  NETPERF_TIMESTAMP_T    now;

  /* this aught to be enough to hold it - modulo stuff like large
     requests on TCP connections... and we make it of type uint16_t so
     we don't have to worry about alignment when we pull shorts out of
     it.  there is a crass joke there somewhere. raj 2005-11-18 */
  uint16_t          request_buffer[NS_PACKETSZ/sizeof(uint16_t)];  


  HISTOGRAM_VARS;

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  my_data   = test->test_specific_data;

  while (NO_STATE_CHANGE(test) &&
	 keep_going) {
    /* this will put as many requests out there as we are allowed to
       have, and tell us how many it sent.  one of these days, we will
       start tracking how many we sent in addition to how many
       completed. */
    ret = send_dns_requests(test);
    if (ret < 0) {
      /* if something bad happened, we likely don't want to try to
	 poll */
      keep_going = 0;
      continue;
    }

    /* recv the request for the test, but first we really need some sort
       of timeout on a poll call or whatnot... */

    fds.fd = my_data->query_socket;
    fds.events = POLLIN;
    fds.revents = 0;

    /* I wonder if we want a different (presumably smaller) poll
       timeout from the DNS request timeout, if for no other reason
       than to make the test a triffle more responsive to state
       changes? raj 2005-11-30 */
    ret = poll(&fds,1,my_data->timeout);

    switch (ret) {
    case -1: 
      /* something bad happened */
      keep_going = 0;
      report_test_failure(test,
			  (char *)__func__,
			  DNSE_DATA_RECV_ERROR,
			  "poll_error");
    
      break;
    case 0:
      if (test->debug) {
	fprintf(test->where,
		"TIMEOUT in %s num_outstanding %d\n",
		(char *)__func__,
		my_data->num_outstanding);
	fflush(test->where);
      }
      /* we had a timeout. invalidate the existing request(s) as
	 appropriate so we will ignore it if it was simply delayed.
	 status_entry should still be valid.  we only need to go back
	 at most num_outstanding requests */
      scan_for_expired_requests(test);
      break;
    case 1:
      /* there is actually something there to recv :) */
      bytes_left = NS_PACKETSZ; /* we'll do something clever inside the
				   loop to handle UDP vs TCP. */
      rsp_ptr = request_buffer;  /* until we discover it is a bug, re-use
				    the reqeust buffer */
      while (bytes_left > 0) {
	if ((len=recv(my_data->query_socket,
		      rsp_ptr,
		      bytes_left,
		      0)) != 0) {
	  /* this macro hides windows differences */
	  if (CHECK_FOR_RECV_ERROR(len)) {
	    keep_going = 0;
	    report_test_failure(test,
				(char *)__func__,
				DNSE_DATA_RECV_ERROR,
				"data_recv_error");
	    break;
	  }
	  /* do we have more to read this time around? */
	  if (my_data->use_tcp == 0) {
	    /* we are UDP */
	    response_len = len;
	    bytes_left = 0;
	  }
	  else {
	    /* not quite sure what to do here - probably have to parse the
	       packet a bit more, update response_len etc... */
	  }
	} 
	else {
	  /* len is 0 the connection was closed exit the while loop */
	  keep_going = 0;
	  break;
	}
      }
      response_id = rsp_ptr[0];
      status_entry  = find_status_entry(test, response_id);

      if ((status_entry) &&
	  (status_entry->active)) {
	if (test->debug) {
	  NETPERF_TIMESTAMP_T now;
	  netperf_timestamp(&now);
	  fprintf(test->where,
		  "COMPLETING message %d in %s after %d ms\n",
		  response_id,
		  (char *)__func__,
		  delta_milli(&(status_entry->sent_time),&now));
	  fflush(test->where);
	}

	/* this is what we want to see */
	/* code to timestamp enabled by HISTOGRAM */
	HIST_TIMESTAMP(&time_two);
	HIST_ADD(my_data->time_hist,
		 delta_milli(&(status_entry->sent_time),&time_two));
	/* my_data->stats.named.responses_received++; */
	/* so we can continue to "leverage" the nettest_bsd reporter for
	   now. raj 2005-11-18 */
	my_data->stats.named.trans_sent++;
	my_data->stats.named.trans_received++;
	/* my_data->stats.named.response_bytes_received += response_len; */
	/* hey dummy, don't forget to clear the entry... raj 2005-11-22 */
	memset(status_entry,0,sizeof(dns_request_status_t));
      }
      else {
	/* is this bad?  well, if we were in a transition from LOAD to
	   MEAS state it could happen I suppose so for now we will simply
	   ignore the message */
	if (test->debug) {
	  fprintf(test->where,
		  "IGNORING message %d in %s entry inactive or not present\n",
		  response_id,
		  (char *)__func__);
	  fflush(test->where);
	}
      }

      if (len == 0) {
	/* how do we deal with a closed connection in the measured
	   state?  well, we'll deal with that when we actually start
	   doing requests over TCP. raj 2005-11-29 */
	keep_going = 0;
	report_test_failure(test,
			    (char *)__func__,
			    DNSE_DATA_CONNECTION_CLOSED_ERROR,
			    "data connection closed during TEST_MEASURE state");
      }
      break;
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
send_dns_rr_load(test_t *test)
{
  uint32_t          new_state;
  int               ret;
  int               len;
  int               bytes_left;
  int               req_size;
  uint16_t         *rsp_ptr;
  dns_data_t       *my_data;
  struct pollfd     fds;
  int               keep_going=1;
  uint16_t          response_id;
  dns_request_status_t *status_entry;
  uint16_t           request_buffer[NS_PACKETSZ/sizeof(uint16_t)];

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  my_data   = test->test_specific_data;

  while (NO_STATE_CHANGE(test) &&
	 keep_going) {

    ret = send_dns_requests(test);
    if (ret < 0) {
      /* if something bad happened, we likely don't want to try to
	 poll */
      keep_going = 0;
      continue;
    }

    fds.fd = my_data->query_socket;
    fds.events = POLLIN;
    fds.revents = 0;

    ret = poll(&fds,1,my_data->timeout); 

    switch (ret) {
    case -1: 
      /* something bad happened */
      keep_going = 0;
      report_test_failure(test,
			  (char *)__func__,
			  DNSE_DATA_RECV_ERROR,
			  "poll_error");
    
      break;
    case 0:
      /* we had a timeout. */
      if (test->debug) {
	fprintf(test->where,
		"TIMEOUT in %s num_outstanding %d\n",
		(char *)__func__,
		my_data->num_outstanding);
	fflush(test->where);
      }
      scan_for_expired_requests(test);
      break;
    case 1:
      /* recv the request for the test */
      rsp_ptr    = request_buffer;
      bytes_left = NS_PACKETSZ;
      while (bytes_left > 0) {
	if ((len=recv(my_data->query_socket,
		      rsp_ptr,
		      bytes_left,
		      0)) != 0) {
	  /* this macro hides windows differences */
	  if (CHECK_FOR_RECV_ERROR(len)) {
	    keep_going = 0;
	    report_test_failure(test,
				(char *)__func__,
				DNSE_DATA_RECV_ERROR,
				"data_recv_error");
	    break;
	  }
	  /* do we have more to read this time around? */
	  if (!my_data->use_tcp) {
	    /* we are UDP */
	    bytes_left = 0;
	  }
	  else {
	    /* not quite sure what to do here - probably have to parse
	       the packet a bit more, but that will happen once we
	       support sending requests over TCP, for UDP, we should
	       never really get here*/
	  }
	}
	else {
	  /* len is 0 the connection was closed exit the while loop */
	  keep_going = 0;
	  break;
	}
      }

      response_id = rsp_ptr[0];
      status_entry  = find_status_entry(test, response_id);

      if ((status_entry) &&
	  (status_entry->active)) {
	if (test->debug) {
	  NETPERF_TIMESTAMP_T now;
	  netperf_timestamp(&now);
	  fprintf(test->where,
		  "COMPLETING message %d in %s after %d ms\n",
		  response_id,
		  (char *)__func__,
		  delta_milli(&(status_entry->sent_time),&now));
	  fflush(test->where);
	}

	/* since we are in the load state we do not actually record
	   any completed transactions, just go ahead and clear the
	   status_entry. raj 2005-11-30 */
	memset(status_entry,0,sizeof(dns_request_status_t));
      }
      else {
	/* is this bad?  well, if we were in a transition from LOAD to
	   MEAS state it could happen I suppose so for now we will simply
	   ignore the message */
	if (test->debug) {
	  fprintf(test->where,
		  "IGNORING message %d in %s entry inactive or not present\n",
		  response_id,
		  (char *)__func__);
	  fflush(test->where);
	}
      }

      break;

    }
  }
  new_state = CHECK_REQ_STATE;
  if ((len == 0) ||
      (new_state == TEST_IDLE)) {
    send_dns_rr_idle_link(test,len);
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
  return(dns_test_clear_stats(GET_TEST_DATA(test)));
}


xmlNodePtr
recv_tcp_rr_get_stats(test_t *test)
{
  return( dns_test_get_stats(test));
}

void
recv_tcp_rr_decode_stats(xmlNodePtr stats,test_t *test)
{
  dns_test_decode_stats(stats,test);
}


int
send_dns_rr_clear_stats(test_t *test)
{
  return(dns_test_clear_stats(GET_TEST_DATA(test)));
}

xmlNodePtr
send_dns_rr_get_stats(test_t *test)
{
  return( dns_test_get_stats(test));
}

void
send_dns_rr_decode_stats(xmlNodePtr stats,test_t *test)
{
  dns_test_decode_stats(stats,test);
}




/* This routine implements the UDP request/response test */
/* (a.k.a. rr) for the sockets interface. It receives its */
/* parameters via the xml node contained in the test structure */
/* output to the standard output. */
/* results are collected by the procedure send_dns_rr_get_stats */

void
send_dns_rr(test_t *test)
{
  uint32_t state, new_state;

  /* this may be interesting as we may have to move things around a
     bit - we may not necessarily know if we are using UDP or TCP at
     this point in the parsing... raj 2005-11-18 */
  dns_test_init(test, SOCK_DGRAM, IPPROTO_UDP);

  NETPERF_DEBUG_ENTRY(test->debug,test->where);

  state = GET_TEST_STATE;
  while ((state != TEST_ERROR) &&
         (state != TEST_DEAD )) {
    switch(state) {
    case TEST_PREINIT:
      send_dns_rr_preinit(test);
      new_state = TEST_INIT;
      break;
    case TEST_INIT:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        new_state = send_dns_rr_init(test);
      }
      break;
    case TEST_IDLE:
      new_state = CHECK_REQ_STATE;
      if (new_state == TEST_IDLE) {
        sleep(1);
      }
      break;
    case TEST_MEASURE:
      new_state = send_dns_rr_meas(test);
      break;
    case TEST_LOADED:
      new_state = send_dns_rr_load(test);
      break;
    default:
      break;
    } /* end of switch */
    set_test_state(test, new_state);
    state = GET_TEST_STATE;
  } /* end of while */
  wait_to_die(test);
} /* end of send_dns_rr */


/*  This implementation of report_dns_test_results will generate strange
    results if transaction count and throughput tests are included in the
    same test set. The first test in the set sets the headers and algorithm
    for computing service demand */

void
dns_test_results_init(tset_t *test_set,char *report_flags,char *output)
{
  dns_results_t *rd;
  FILE          *outfd;
  int            max_count;

  rd        = test_set->report_data;
  max_count = test_set->confidence.max_count;
  
  if (output) {
    if (test_set->debug) {
      fprintf(test_set->where,
              "dns_test_results_init: report going to file %s\n",
              output);
      fflush(test_set->where);
    }
    outfd = fopen(output,"a");
  } else {
    if (test_set->debug) {
      fprintf(test_set->where,
              "report_dns_test_results: report going to file stdout\n");
      fflush(test_set->where);
    }
    outfd = stdout;
  }
  /* allocate and initialize report data */
  rd = malloc(sizeof(dns_results_t) + 7 * max_count * sizeof(double));
  if (rd) {
    memset(rd, 0,
           sizeof(sizeof(dns_results_t) + 7 * max_count * sizeof(double)));
    rd->max_count      = max_count;
    rd->results        = &(rd->results_start);
    rd->xmit_results   = &(rd->results[max_count]);
    rd->recv_results   = &(rd->xmit_results[max_count]);
    rd->trans_results  = &(rd->recv_results[max_count]);
    rd->utilization    = &(rd->trans_results[max_count]);
    rd->servdemand     = &(rd->utilization[max_count]);
    rd->run_time       = &(rd->servdemand[max_count]);
    rd->result_minimum = MAXDOUBLE;
    rd->result_maximum = MINDOUBLE;
    rd->outfd          = outfd;
    rd->sd_denominator = 0.0;
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
    if (test_set->debug) {
      rd->print_run  = 1;
      rd->print_test = 1;
    }
    test_set->report_data = rd;
  } else {
    /* could not allocate memory can't generate report */
    fprintf(outfd,
            "dns_test_results_init: malloc failed can't generate report\n");
    fflush(outfd);
    exit;
  }
}

void
process_test_stats(tset_t *test_set, xmlNodePtr stats, xmlChar *tid)
{
  int            i;
  int            count;
  int            index;
  FILE          *outfd;
  dns_results_t *rd;

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
        sscanf(value_str,"%llx",&x);
        test_cntr[i] = (double)x;
      }
    } else {
      test_cntr[i] = 0.0;
    }
    if (test_set->debug) {
      fprintf(test_set->where,"\t%12s test_cntr[%2d] = %10g\t'%s'\n",
              cntr_name[i], i, test_cntr[i],
              xmlGetProp(stats, (const xmlChar *)cntr_name[i]));
    }
  }
  elapsed_seconds = test_cntr[TST_E_SEC] + test_cntr[TST_E_USEC]/1000000;
  xmit_rate       = test_cntr[TST_X_BYTES]*8/(elapsed_seconds*1000000);
  recv_rate       = test_cntr[TST_R_BYTES]*8/(elapsed_seconds*1000000);
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
    if (xmit_rate > 0.0 || recv_rate > 0.0) {
      rd->sd_denominator = 1000000.0/(8.0*1024.0);
    } else {
      rd->sd_denominator = 1.0;
    }
  }
  if (test_set->debug) {
    fprintf(test_set->where,"\tsd_denominator = %f\n",rd->sd_denominator);
    fflush(test_set->where);
  }
  if (rd->sd_denominator != 1.0) {
    result = recv_rate + xmit_rate;
  } else {
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
    } else {
      fprintf(outfd,"%10.2f ",result);                /* 19,11 */
    }
    fprintf(outfd,"\n");
    fflush(outfd);
  }
  /* end of printing dns per test instance results */
}

void
process_sys_stats(tset_t *test_set, xmlNodePtr stats, xmlChar *tid)
{
  int            i;
  int            count;
  int            index;
  FILE          *outfd;
  dns_results_t *rd;
  double         elapsed_seconds;
  double         sys_util;
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
        sscanf(value_str,"%llx",&x);
        sys_cntr[i] = (double)x;
      }
    } else {
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
    if (rd->sd_denominator != 1.0) {
      fprintf(outfd,"%24s","");                       /* 19,24*/
    } else {
      fprintf(outfd,"%11s","");                       /* 19,11*/
    }
    fprintf(outfd,"%7.1e ",calibration);              /* 43,8 */
    fprintf(outfd,"%6.3f ",local_idle*100.0);         /* 51,7 */
    fprintf(outfd,"%6.3f ",local_busy*100.0);         /* 58,7 */
    fprintf(outfd,"\n");                              /* 79,1 */
    fflush(outfd);
  }
  /* end of printing sys stats instance results */
}

void
process_stats_for_run(tset_t *test_set)
{
  dns_results_t *rd;
  test_t        *test;
  tset_elt_t    *set_elt;
  xmlNodePtr     stats;
  xmlNodePtr     prev_stats;
  int            count; 
  int            index; 
 
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
    fprintf(test_set->where, "%s count = %d\n", (char *) __func__, count);
    fflush(test_set->where);
  }

  rd->results[index]       =  0.0;
  rd->xmit_results[index]  =  0.0;
  rd->recv_results[index]  =  0.0;
  rd->utilization[index]   =  0.0;
  rd->servdemand[index]    =  0.0;
  rd->run_time[index]      =  0.0;

  while (set_elt != NULL) {
    int stats_for_test;
    test    = set_elt->test;
    stats   = test->received_stats->xmlChildrenNode;
    if (test_set->debug) {
      if (stats) {
        fprintf(test_set->where,
                "\ttest %s has '%s' statistics\n",
                test->id,stats->name);
      } else {
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
        process_sys_stats(test_set, stats, test->id);
        stats_for_test++;
      }
      if(!xmlStrcmp(stats->name,(const xmlChar *)"test_stats")) {
        /* process test statistics */
        process_test_stats(test_set, stats, test->id);
        stats_for_test++;
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
              (char *)__func__);
      fprintf(test_set->where,
              "exiting netperf now!!\n");
      fflush(test_set->where);
      exit;
    }
    set_elt = set_elt->next;
  }
  if (rd->result_minimum > rd->results[index]) {
    rd->result_minimum = rd->results[index];
  }
  if (rd->result_maximum < rd->results[index]) {
    rd->result_maximum = rd->results[index];
  }
}

void
update_results_and_confidence(tset_t *test_set)
{
  dns_results_t *rd;
  double         confidence;
  
  rd        = test_set->report_data;

    /* calculate confidence and summary result values */
  confidence                    = get_confidence(rd->run_time,
                                      &(test_set->confidence),
                                      &(rd->ave_time));
  rd->result_confidence         = get_confidence(rd->results,
                                      &(test_set->confidence),
                                      &(rd->result_measured_mean));
  rd->cpu_util_confidence       = get_confidence(rd->utilization,
                                      &(test_set->confidence),
                                      &(rd->cpu_util_measured_mean));
  rd->service_demand_confidence = get_confidence(rd->servdemand,
                                      &(test_set->confidence),
                                      &(rd->service_demand_measured_mean));
  if (rd->result_confidence > rd->cpu_util_confidence) {
    if (rd->cpu_util_confidence > rd->service_demand_confidence) {
      confidence  = rd->service_demand_confidence;
    } else {
      confidence  = rd->cpu_util_confidence;
    }
  } else {
    if (rd->result_confidence > rd->service_demand_confidence) {
      confidence  = rd->service_demand_confidence;
    } else {
      confidence  = rd->result_confidence;
    }
  }
  test_set->confidence.value = confidence;
}

void
print_run_results(tset_t *test_set)
{
  dns_results_t *rd;
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
    } else {
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
  } else {
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

void
print_results_summary(tset_t *test_set)
{
  dns_results_t *rd;
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
  } else {
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
  } else {
    fprintf(outfd,"%7.0f ",rd->result_measured_mean);             /* 19,8 */
    fprintf(outfd,"%7.2f ",rd->result_confidence);                /* 27,8 */
    fprintf(outfd,"%7.0f ",rd->result_minimum);                   /* 35,8 */
    fprintf(outfd,"%7.0f ",rd->result_maximum);                   /* 43,8 */
  }
  fprintf(outfd,"%6.4f ",rd->cpu_util_measured_mean);             /* 51,7 */
  fprintf(outfd,"%6.4f ",rd->cpu_util_confidence);                /* 58,7 */
  fprintf(outfd,"%6.3f ",rd->service_demand_measured_mean);       /* 65,7 */
  fprintf(outfd,"%6.3f ",rd->service_demand_confidence);          /* 72,7 */
  fprintf(outfd,"\n");                                            /* 79,1 */
  fflush(outfd);
}

void
report_dns_test_results(tset_t *test_set, char *report_flags, char *output)
{
  dns_results_t *rd;
  int count;
  int max_count;
  int min_count;

  rd  = test_set->report_data;

  if (rd == NULL) {
    dns_test_results_init(test_set,report_flags,output);
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
  if (count == max_count) {

/*  if ((count == max_count) || 
      ((rd->confidence >= 0) && (count >= min_count))) */

    print_results_summary(test_set);
  }
} /* end of report_dns_test_results */

