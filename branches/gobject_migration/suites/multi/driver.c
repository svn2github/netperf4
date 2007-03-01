#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

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


  
int
main(int argc, char *argv[]) {

  int sock;
  int ret;
  int trans = 0;
  char buf[128];

  struct sockaddr_in to;

  sock = socket(AF_INET, SOCK_STREAM, 0);

  if (sock < 0) {
    perror("socket");
    exit(-1);
  }

  bzero(&to,sizeof(to));

  to.sin_family = AF_INET;
  to.sin_port = htons(atoi(argv[2]));
  to.sin_addr.s_addr = inet_addr(argv[1]);

  ret = connect(sock,(struct sockaddr *)&to,sizeof(to));

  if (ret < 0) {
    perror("connect");
    exit(-1);
  }
  
  while (1) {

    ret = send(sock,buf,1,0);

    if (ret < 0) {
      break;
    }

    ret = recv(sock,buf,1,0);

    if (ret < 0) {
      break;
    }
    trans++;
  }
  perror("out of loop");
  printf("trans %d\n",trans);
}
