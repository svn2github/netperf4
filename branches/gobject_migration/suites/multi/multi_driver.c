#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

int debug = 0;

#define SOCKET int
#define netperf_socklen_t socklen_t
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
#define CLOSE_SOCKET close

/* connection FLAGS */
#define  CONNECTION_LISTENING  0x1
#define  CONNECTION_READING 0x2
#define  CONNECTION_WRITING 0x4
#define  CONNECTION_CLOSING 0x8

typedef struct connection {
  void         *next;
  void         *prev;
  void         *buffer_base;
  char         *buffer_curr;
  int          sock;
  int          bytes_total;
  int          bytes_remaining;
  int          bytes_completed;
  unsigned int flags;
  int          buffer_size;
} connection_t;


void
dump_addrinfo(FILE *dumploc, struct addrinfo *info,
              char *host, char *port, int family)
{
  struct sockaddr *ai_addr;
  struct addrinfo *temp;
  temp=info;

  fprintf(dumploc,
	  "getaddrinfo returned the following for host '%s' ",
	  host ? (char *)host : "n/a");
  fprintf(dumploc,
	  "port '%s' ",
	  port ? (char *)port : "n/a");
  fprintf(dumploc, "family %d\n", family);
  while (temp) {
    /* I was under the impression that fprintf would be kind with
    things like NULL string pointers, but when porting to Solaris,
    this started dumping core, so we start to workaround it. raj
    2006-06-29 */
    fprintf(dumploc,
	      "\tcannonical name: '%s'\n",
	      temp->ai_canonname ? temp->ai_canonname : "n/a");
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

SOCKET
establish_connection(char *hostname, char *service, int af, netperf_socklen_t *addrlenp)
{
  SOCKET sockfd;
  int error;
  int count;
  int len = *addrlenp;
  int one = 1;
  struct addrinfo hints, *res, *res_temp;

  if (debug) {
    fprintf(stderr,
	    "establish_connection: host '%s' service '%s' af %d socklen %d\n",
	    hostname ? hostname : "n/a",
	    service ? service : "n/a",
	    af,
	    len);
    fflush(stderr);
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = af;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags    = 0;

  count = 0;
  do {
    error = getaddrinfo(hostname,
			service,
			&hints,
			&res);
    count += 1;
    if (error == EAI_AGAIN) {
      if (debug) {
        fprintf(stderr,"Sleeping on getaddrinfo EAI_AGAIN\n");
        fflush(stderr);
      }
      usleep(1000);
    }
  } while ((error == EAI_AGAIN) && (count <= 5));

  if (error) {
    fprintf(stderr,
            "establish_connection: could not resolve host '%s' service '%s'\n",
            hostname,service);
    fprintf(stderr,"\tgetaddrinfo returned %d %s\n",
            error,gai_strerror(error));
    fflush(stderr);
    return(-1);
  }


  if (debug) {
    dump_addrinfo(stderr, res, hostname,
                  service, AF_UNSPEC);
  }

  res_temp = res;

  do {

    sockfd = socket(res_temp->ai_family,
                    res_temp->ai_socktype,
                    res_temp->ai_protocol);
    if (sockfd == INVALID_SOCKET) {
      if (debug) {
        fprintf(stderr,"establish_connection: socket error trying next one\n");
        fflush(stderr);
      }
      continue;
    }
    /* The Windows DDK compiler is quite picky about pointers so we
       cast one as a void to placate it. */
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(void *)&one,sizeof(one)) == 
	SOCKET_ERROR) {
      fprintf(stderr,"establish_connection: SO_REUSEADDR failed\n");
      fflush(stderr);
    }
    if (connect(sockfd, res_temp->ai_addr, res_temp->ai_addrlen) == 0) {
      break;
    }
    fprintf(stderr,"establish_connection: connect error close and try next\n");
    fflush(stderr);
    CLOSE_SOCKET(sockfd);
  } while ( (res_temp = res_temp->ai_next) != NULL );

  if (res_temp == NULL) {
    fprintf(stderr,"establish_connection: allocate server socket failed\n");
    fflush(stderr);
    sockfd = -1;
  } 
  else {
    if (addrlenp) *addrlenp = res_temp->ai_addrlen;
  }

  fcntl(sockfd,F_SETFL, O_NONBLOCK);

  freeaddrinfo(res);

  return (sockfd);

}

typedef struct event_state_select {
  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;
  int    numfds;
  int    maxfd;
  int    minfd;
} event_state_select_t;

void *
init_event_state() {
  event_state_select_t *temp;

  if (debug) {
    fprintf(stderr,"Initializing event state\n");
  }
  temp = (event_state_select_t *)malloc(sizeof(event_state_select_t));
  if (temp) {
    FD_ZERO(&(temp->readfds));
    FD_ZERO(&(temp->writefds));
    FD_ZERO(&(temp->exceptfds));
  }
  temp->numfds = 0;
  temp->maxfd  = -1;
  temp->minfd = -1;
  if (debug) {
    fprintf(stderr,
	    "Initialized event state at %p, readfds at %p\n",
	    temp,
	    &(temp->readfds));
  }
  return temp;
}

int
clear_all_events(void *event_state, SOCKET sock) {

  int i;
  event_state_select_t *temp = event_state;

  if (debug) {
    fprintf(stderr,
	    "about to clear sock %d from the event set at %p\n",
	    sock, 
	    event_state);
  }

  FD_CLR(sock,&(temp->readfds));
  FD_CLR(sock,&(temp->writefds));
  FD_CLR(sock,&(temp->exceptfds));
  
  temp->numfds -= 1;

  if (debug) {
    fprintf(stderr,
	    "initiating maxfd_check sock %d temp->maxfd %d\n",
	    sock,
	    temp->maxfd);
  }

  if (sock == temp->maxfd) {
    for (i = temp->maxfd; i >= 0; i--) { 
      if (FD_ISSET(i,&(temp->readfds)) ||
	  FD_ISSET(i,&(temp->writefds)) ||
	  FD_ISSET(i, &(temp->exceptfds))) {
	temp->maxfd = i;
	if (debug) {
	  fprintf(stderr,"maxfd is now %d\n",i);
	}
	break;
      }
    }
  }
  else if (sock == temp->minfd) {
    for (i = temp->minfd; i < FD_SETSIZE; i++) {
      if (FD_ISSET(i,&(temp->readfds)) ||
	  FD_ISSET(i,&(temp->writefds)) ||
	  FD_ISSET(i, &(temp->exceptfds))) {
	temp->minfd = i;
	if (debug) {
	  fprintf(stderr,"maxfd is now %i\n",i);
	}
	break;
      }
    }
  }

  return 0;

}
int
set_readable_event(void *event_state, SOCKET sock) {
  event_state_select_t *temp;
  temp = event_state;
  if (debug) {
    fprintf(stderr,"requesting readability for socket %d\n",sock);
  }
  if (!FD_ISSET(sock,&(temp->readfds))) {
    FD_SET(sock,&(temp->readfds));
    temp->numfds += 1;
    temp->maxfd = (sock > temp->maxfd) ? sock : temp->maxfd;
    temp->minfd = (sock < temp->minfd) ? sock : temp->minfd;
  }
  if (debug) {
    fprintf(stderr,"minfd is now %d maxfd is now %d\n",
	    temp->minfd,
	    temp->maxfd);
  }
  return 0;
}

int
set_writable_event(void *event_state, SOCKET sock) {
  event_state_select_t *temp;
  temp = event_state;
  if (debug) {
    fprintf(stderr,"requesting writability for socket %d\n",sock);
  }
  if (!FD_ISSET(sock,&(temp->writefds))) {
    FD_SET(sock,&(temp->writefds));
    temp->numfds += 1;
    temp->maxfd = (sock > temp->maxfd) ? sock : temp->maxfd;
    temp->minfd = (sock < temp->maxfd) ? sock : temp->maxfd;
  }
  if (debug) {
    fprintf(stderr,"maxfd is now %d\n",temp->maxfd);
  }
  return 0;
}

typedef struct uber_state {
  connection_t *connection_list;
  void         *event_state;
  int          rdwr_since_accept;
} uber_state_t;

connection_t *
find_connection(uber_state_t *uber_state, SOCKET sock) {

  connection_t *temp;

  temp = uber_state->connection_list; 

  if (debug) {
    fprintf(stderr,
	    "searching connection list starting from %p for %d\n",temp,sock);
  }
  while (temp) {
    if (debug) {
      fprintf(stderr,"examining connection %p whose sock is %d\n",
	      temp,
	      temp->sock);
    }
    if (temp->sock == sock) break;
    temp = temp->next;
  }

  if (debug) {
    fprintf(stderr,"returning %p from find_connection\n",temp);
  }
  return temp;
  
}
int
wait_for_events_and_walk(uber_state_t *uber_state) {
  struct timeval timeout;
  event_state_select_t *event_state = uber_state->event_state;

  fd_set loc_readfds;
  fd_set loc_writefds;

  connection_t *temp_connection;

  int ret;
  int i;
  int did_something;

  timeout.tv_sec = 2;
  timeout.tv_usec = 0;

  if (debug) {
    fprintf(stderr,"about to go into select\n");
  }

  /* rather blunt, but should be effective for now */
  memcpy(&loc_readfds, &(event_state->readfds),sizeof(fd_set));
  memcpy(&loc_writefds, &(event_state->writefds),sizeof(fd_set));

  ret = select(event_state->maxfd + 1,
	       &loc_readfds,
	       &loc_writefds,
	       NULL,   /* ignore exception for now */
	       &timeout);

  if (debug) {
    fprintf(stderr,
	    "select returned %d errno %d minfd %d maxfd %d\n",
	    ret,
	    errno,
	    event_state->minfd,
	    event_state->maxfd);
  }

  if (ret > 0) {
    /* first, the reads and accepts */
    for (i = event_state->minfd; i <= event_state->maxfd; i++) {
      if (FD_ISSET(i,&loc_readfds)) {
	temp_connection = find_connection(uber_state,i);
	if (debug) {
	  fprintf(stderr,
		  "fd %d is readable on connection %p\n",
		  i,
		  temp_connection);
	}
	/* is this our listen endpoint or something else? */
	if (temp_connection->flags & CONNECTION_LISTENING ) {
	  /* accept a connection */
	  SOCKET new_sock;
	  new_sock = accept_new_connection(uber_state,
					   temp_connection);
	  if (INVALID_SOCKET != new_sock) {
	    /* on the very good chance that this new connection is for
	       an FD higher than our listen endpoint, we set
	       loc_readfds so we go ahead and try to read from the new
	       connection when we get to that point in our walk of the
	       fdset returned from select(). */
	    FD_SET(new_sock,&loc_readfds);
	  }
	  /* if we got one connection, we should see about draining
	     the accept queue, eventually module some limit on the
	     number of concurrent connections we will allow */
	  while (new_sock >= 0) {
	    new_sock = accept_new_connection(uber_state,
					     temp_connection);
	    if (INVALID_SOCKET != new_sock) {
	      /* on the very good chance that this new connection is for
		 an FD higher than our listen endpoint, we set
		 loc_readfds so we go ahead and try to read from the new
		 connection when we get to that point in our walk of the
		 fdset returned from select(). */
	      FD_SET(new_sock,&loc_readfds);
	    }
	  }
	}
	else if (temp_connection->flags & CONNECTION_READING) {
	  /* read some bytes */
	}
	else {
	  /* something screwy happened */
	  fprintf(stderr,
		  "YO, we got readable on connection %p but we weren't reading\n",
		  temp_connection);
	}
      }
    }
    
    /* now the writes */
    for (i = event_state->minfd; i <= event_state->maxfd; i++) {
      if (FD_ISSET(i,&loc_writefds)) {
	temp_connection = find_connection(uber_state,i);
	if (debug > 2) {
	  fprintf(stderr,
		  "fd %d is writable on connection %p\n",i,temp_connection);
	}
      }
    }
  }

  return 0;
}

int
remove_old_connection(uber_state_t *uber_state, connection_t *old_connection) {

  connection_t *prev_conn = NULL;
  connection_t *curr_conn;

  curr_conn = uber_state->connection_list;

  while (NULL != curr_conn) {
    if (curr_conn == old_connection) {
      /* we have a match */
      if (NULL != prev_conn) {
	/* we were somewhere in the middle, or at the end */
	prev_conn->next = curr_conn->next;
      }
      else {
	/* we were at the beginning */
	uber_state->connection_list = curr_conn->next;
      }
      break;
    }
    else {
      prev_conn = curr_conn;
      curr_conn = curr_conn->next;
    }
  }
  /* whether we have found it in the list or not, we want to free the
     buffers and the connection entry */
  free(old_connection->buffer_base);
  free(old_connection);
  
  return 0;

}
connection_t *
add_new_connection(uber_state_t *uber_state, SOCKET init_socket, int flags, int initial_buf_size) {
  connection_t *temp_conn;

  temp_conn = malloc(sizeof(connection_t));
  if (NULL != temp_conn) {
    temp_conn->sock = init_socket;
    temp_conn->bytes_total = 1;
    temp_conn->bytes_remaining = 1;
    temp_conn->bytes_completed = 0;
    temp_conn->flags = flags;
    temp_conn->buffer_size = initial_buf_size;
    temp_conn->buffer_base = NULL;
    temp_conn->buffer_size = 0;
    /* if the caller asked for an initial buffer to be allocated,
       allocate one */
    if (initial_buf_size > 0) {
      temp_conn->buffer_base = malloc(initial_buf_size);
      if (NULL != temp_conn->buffer_base) {
	temp_conn->buffer_size = initial_buf_size;
      }
    }
    /* doesn't matter here if buffer_base is NULL */
    temp_conn->buffer_curr = temp_conn->buffer_base;
    temp_conn->next = uber_state->connection_list;
    uber_state->connection_list = temp_conn;
  }
  if (flags & (CONNECTION_READING | CONNECTION_LISTENING)) {
    set_readable_event(uber_state->event_state,init_socket);
  }
  if (flags & CONNECTION_WRITING) {
    set_writable_event(uber_state->event_state,init_socket);
  }

  return temp_conn;
}

int
accept_new_connection(uber_state_t *uber_state, connection_t *listen_connection) 
{
  SOCKET new_conn;
  connection_t *new_connection;

  if (debug) {
    fprintf(stderr,
	    "about to accept on connection %p fd %d\n",
	    listen_connection,
	    listen_connection->sock);
  }

  /* we don't care if the accept actually accepted a new connection,
     just that we tried calling accept. */
  uber_state->rdwr_since_accept = 0;


  new_conn = accept(listen_connection->sock,NULL,NULL);

  if (INVALID_SOCKET != new_conn) {
    
    /* one day we will want to see if this is inherited across
       accept() calls, but for now we just go ahead and make the
       call */

    fcntl(new_conn,F_SETFL, O_NONBLOCK);

    new_connection = add_new_connection(uber_state,
					new_conn,
					CONNECTION_READING,
					128);
    if (debug) {
      fprintf(stderr,
	      "accepted a connection to %p on fd %d\n",
	      new_connection,
	      new_conn);
    }
  }

  return new_conn;
}

int
close_a_connection(uber_state_t *uber_state, connection_t *close_connection) {

  int ret;

  if (debug) {
    fprintf(stderr,
	    "about to close connection %p\n",
	    close_connection);
  }

  ret = close(close_connection->sock);
  if (debug) {
    fprintf(stderr,"close_connection thinks event_state is %p\n",
	    uber_state->event_state);
  }
  ret = clear_all_events(uber_state->event_state,close_connection->sock);
  /* mark the connection for removal at some later time, don't remove
     it here because we may be called from a loop that is walking the
     connection list */
  close_connection->flags = CONNECTION_CLOSING;

  /* ret = remove_old_connection(uber_state, close_connection); */

  return 0;
}

int
write_on_connection(uber_state_t *uber_state, connection_t *write_connection) {

  int bytes_written;
  int did_something=1;

  if (debug > 2) {
    fprintf(stderr,
	    "about to write on connection %p\n",write_connection);
  }

  /* actually writing bytes doesn't matter, just that we tried */
  uber_state->rdwr_since_accept += 1;
  
  bytes_written = send(write_connection->sock,
		       write_connection->buffer_curr,
		       write_connection->bytes_remaining,
		       0);

  if (bytes_written > 0) {
    /* wrote something, was it everything? */
    write_connection->bytes_remaining -= bytes_written;
    if (write_connection->bytes_remaining > 0) {
      write_connection->buffer_curr = 
	write_connection->buffer_curr + bytes_written;
      write_connection->flags = CONNECTION_WRITING;
    }
    else {
      /* we wrote everything we wanted. set things up so we read next
	 time around */
      write_connection->buffer_curr = write_connection->buffer_base;
      write_connection->bytes_remaining = write_connection->bytes_total;
      write_connection->flags = CONNECTION_READING;
    }
  }
  else if (bytes_written == 0) {
    /* didn't do anything */
    did_something = 0;
  }
  else {
    /* it was < 0 - wonder what the error was? */
    did_something = 0;
  }
  if (debug > 2) {
    fprintf(stderr,
	    "wrote %d bytes on connection %p\n",
	    bytes_written,
	    write_connection);
  }
  return did_something;
}

int
read_on_connection(uber_state_t *uber_state, connection_t *read_connection) {

  int bytes_read;
  char *temp_buf;
  int did_something = 1;

  if (debug > 2) {
    fprintf(stderr,
	    "about to read <= %d bytes on connection %p\n",
	    read_connection->bytes_remaining,
	    read_connection);
  }

  /* we don't actually care if the recv returns any bytes, just that
     we called recv. */
  uber_state->rdwr_since_accept += 1;

  bytes_read = recv(read_connection->sock,
		    read_connection->buffer_curr,
		    read_connection->bytes_remaining,
		    0);

  if (debug > 2) {
    fprintf(stderr,
	    "read %d bytes from connection %p\n",
	    bytes_read,
	    read_connection);
  }

  if (bytes_read > 0) {
    /* got some bytes */
    read_connection->bytes_remaining -= bytes_read;
    /* did we get all we wanted? */
    if (!read_connection->bytes_remaining) {
      /* yes we did. see about writing something back. if the write
	 call was able to write the entire response in one swell foop
	 it will leave us in CONNECTION_READING mode. otherwise it
	 will switch us to CONNECTION_WRITING mode until we complete
	 our writes.  we can do this because we are a program that
	 does one thing at a time  */
      read_connection->bytes_remaining = read_connection->bytes_total;
      read_connection->bytes_completed = 0;
      write_on_connection(uber_state, read_connection);
    }
  }
  else if (bytes_read == 0) {
    uber_state->rdwr_since_accept += 1;
    /* remote closed - should this count as having doen work or not? */
    close_a_connection(uber_state,read_connection);
  }
  else {
    /* it was < 0 we should reall check against EAGAIN */
    /* errors or EAGAIN don't count as having done something */
    did_something = 0;
  }

  return did_something;

}

int
walk_connection_list(uber_state_t *uber_state) {

  connection_t *temp_connection = uber_state->connection_list;
  connection_t *del_connection;

  int did_something = 0;
  int ret;

  while(temp_connection) {
    switch (temp_connection->flags) {
    case CONNECTION_LISTENING: 
      if (debug > 2) {
	fprintf(stderr,
		"connection at %p is a LISTEN connection\n",temp_connection);
      }
      /* only bother doing an accept on the blind walk of the list
	 when we've done some transactions.  otherwise, the drop into
	 the select loop will catch things for us.  we ass-u-me that
	 for the test the actual connection rate is << the transaction
	 rate.  otherwise we'll need some additional heuristics -
	 perhaps domething involving how long it has been since an
	 accept() or how many consecutive accepts without a new
	 connection we've made. */
      if (uber_state->rdwr_since_accept > 1000) {
	ret = accept_new_connection(uber_state,temp_connection);
	/* did we get one, and are there any more? */
	while (ret > 0) {
	  /* if we came-in here, it means we got a connection above, so
	     we did do something.  it probably isn't worth the added
	     conditional to only set did_something once... */
	  did_something = 1;
	  ret = accept_new_connection(uber_state,temp_connection);
	}
      }
      temp_connection = temp_connection->next;
      break;
    case CONNECTION_READING:
      if (debug > 2) {
	fprintf(stderr,
		"connection at %p is a READING connection\n",temp_connection);
      }
      ret = read_on_connection(uber_state,temp_connection);
      if (ret > 0) did_something = 1;
      temp_connection = temp_connection->next;
      break;
    case CONNECTION_WRITING:
      if (debug > 2) {
	fprintf(stderr,
		"connection at %p is a WRITING connection\n",temp_connection);
      }
      ret = write_on_connection(uber_state,temp_connection);
      if (ret > 0) did_something = 1;
      temp_connection = temp_connection->next;
      break;
    case CONNECTION_CLOSING:
      /* ok, just how should we deal with this then? */
      del_connection = temp_connection;
      temp_connection = temp_connection->next;
      remove_old_connection(uber_state, del_connection);
      break;
    }
  }
  return did_something;
}

  
int
main(int argc, char *argv[]) {

  int temp;
  int loops = 0;
  int did_work;
  socklen_t addrlen = sizeof(struct sockaddr_storage);
  uber_state_t *uber_state;
  event_state_select_t *mumble;
  connection_t *connection_list;

  connection_t *temp_connection;

  uber_state = (uber_state_t *)malloc(sizeof(uber_state_t));
  uber_state->connection_list = NULL;
  uber_state->event_state = init_event_state();
  mumble = uber_state->event_state;
  uber_state->rdwr_since_accept = 0;

  fprintf(stderr,"Hello there, let's generate some transactions. Uberstate %p connection_list %p event_state %p\n", uber_state,uber_state->connection_list,uber_state->event_state);

  for (loops = 0; loops < atoi(argv[3]); loops++) {
    temp = establish_connection(argv[1],argv[2],AF_INET,&addrlen);

    /* initialize our event_state minfd */
    /* mumble->minfd = temp; */

    temp_connection = add_new_connection(uber_state,
					 temp,
					 CONNECTION_WRITING,
					 128);

    fprintf(stderr,"temp_connection is at %p\n",temp_connection);
  }

  do {
    loops++;
    if (debug > 1) {
      fprintf(stderr,"\nabout to walk loop %d\n",loops);
    }
    did_work = walk_connection_list(uber_state);
    if (!did_work) {
      if (debug) {
	fprintf(stderr,
		"walk_connection_list did no work, time to wait\n");
      }
      did_work = wait_for_events_and_walk(uber_state);
    }
  } while (1);

}
