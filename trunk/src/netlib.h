/*

$Id$

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

#ifdef NOTDEF
#include <netdb.h>

#include <poll.h>
#endif

extern  test_t * find_test_in_hash(const xmlChar *id);
extern  void report_test_status(server_t *server);
extern  GenReport get_report_function(xmlNodePtr cmd);

/* do we REALLY want this stuff? */
#ifdef NO_DLOPEN
#include <dl.h>

/* dlopen flags */
#define RTLD_NOW        0x00000001  /* bind immediately */
#define RTLD_LAZY       0x00000002  /* bind deferred */
#define RTLD_GLOBAL     0x00000004  /* symbols are globally visible */
#define RTLD_LOCAL      0x00000008  /* symbols only visible to this load */

static void* dlopen(char* filename, unsigned int flags)
{
  shl_t handle;
  handle = shl_load( filename, BIND_IMMEDIATE | BIND_VERBOSE , 0L );
  return((void *)handle);
}

static void* dlsym(void* handle, const char* name)
{
  int rc;
  void *value = NULL;
  shl_t shl_handle = handle;
  rc = shl_findsym( &shl_handle, name, TYPE_PROCEDURE, &value );
  return(value);
}

static char* dlerror(void)
{
  switch (errno) {
    case ENOEXEC:
      return("The specified file is not a shared library, or a format error was detected.");
    case ENOSYM:
      return("Some symbol required by the shared library could not be found.");
    case EINVAL:
      return("The specified handle or index is not valid or an attempt was made to load a library at an invalid address.");
    case ENOMEM:
      return("There is insufficient room in the address space to load the library.");
    case ENOENT:
      return("The specified library does not exist.");
    case EACCES:
      return("Read or execute permission is denied for the specified library.");
    case 0:
      return("Symbol not found.");
    default:
      return("Unknown error");
  }
}

#else
#include <dlfcn.h>
#endif

/* state machine data structure for process message */

typedef int (*msg_func_t)(xmlNodePtr msg, xmlDocPtr doc, server_t *server);

struct msgs {
  char *msg_name;
  msg_func_t msg_func;
  unsigned int valid_states;
};

void netlib_init();

void display_test_hash();

