/* a very crude replacement for the get_expiration_time routine that
   can be found under HP-UX */

#include <sys/time.h>
#include <time.h>

int get_expiration_time(struct timespec *delta,struct timespec *abstime )
{
  struct timeval now;
  gettimeofday(&now,NULL);
  abstime->tv_sec = now.tv_sec + delta->tv_sec;
  abstime->tv_nsec = (now.tv_usec * 1000) + delta->tv_nsec;
  while (delta->tv_nsec > 1000000000) {
    abstime->tv_sec += 1;
    delta->tv_nsec -= 1000000000;
  }
}
