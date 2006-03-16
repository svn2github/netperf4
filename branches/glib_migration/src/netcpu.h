/* This should define all the common routines etc exported by the
   various netcpu_mumble.c files raj 2005-01-26 */

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

extern void  cpu_util_init(void);
extern void  cpu_util_terminate(void);
extern int   get_cpu_method();

#ifdef WIN32
/* +*+ temp until I figure out what header this is in; I know it's
   there someplace...  */
typedef unsigned __int64    uint64_t;
#endif

extern void  get_cpu_idle(uint64_t *res);
extern float calibrate_idle_rate(int iterations, int interval);
extern float calc_cpu_util_internal(float elapsed);
extern void  cpu_start_internal(void);
extern void  cpu_stop_internal(void);

