# 
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

FULL-PACKAGE-NAME=netperf
VERSION=4.0.0rc2
BUG-REPORT-ADDRESS=netperf-feedback@netperf.org

AC_PREREQ(2.59)
AC_INIT(netperf, 4.0.0rc2)
# use the target version rather than host - one day we may want cross-compile
AC_CANONICAL_TARGET
# AC_CONFIG_SRCDIR([src/hist.h])
AM_INIT_AUTOMAKE([dist-zip])
# AM_CONFIG_HEADER(config.h)
AC_CONFIG_HEADERS(config.h)

AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

AC_CONFIG_LIBOBJ_DIR(src/missing)

# Checks for programs.
AC_PROG_CC

AC_C_CONST

# likely as good a place as any to see about adding stuff to flags I
# wonder if this needs to get any more sophisticated than this?
# normally, these would come from the pkg-config stuff on our
# dependencies, but since we won't always have our dependencies
# compiled with the same compiler as we are using, taking the straight
# --cflags from pkg-config can cause us trouble - in particular, gcc's
# ever so helpful -pthread option.  sigh

case $target in 
     *-*-linux*)
        CPPFLAGS="$CPPFLAGS -D_REENTRANT -D_GNU_SOURCE"
	;;
     *-*-hpux*)
        CPPFLAGS="$CPPFLAGS -D_REENTRANT"
	;;
     *-*-aix*)
        CPPFLAGS="$CPPFLAGS -D_THREAD_SAFE"
esac

# need to check if the compiler is gcc and if so add -pthread 
# to suitable flags

# Checks for libraries.  the check for pthread has been moved to where
# we decide if we are using glib or not.
AC_HAVE_LIBRARY(m)
# AC_HAVE_LIBRARY(dl)

# Checks for header files.
# where we decide if we are using glib or not
AC_HEADER_STDC
# AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h arpa/nameser.h arpa/nameser_compat.h errno.h fcntl.h float.h inttypes.h limits.h malloc.h math.h memory.h netdb.h netinet/in.h netinet/tcp.h poll.h pwd.h pthread.h resolv.h sched.h signal.h stdint.h stdio.h stdlib.h string.h strings.h sys/dk.h sys/ioctl.h sys/param.h sys/pstat.h sys/resource.h sys/socket.h sys/stat.h sys/time.h sys/types.h sys/uio.h unistd.h values.h ])

# Some platforms require these.  There may be a better way.
AC_HAVE_LIBRARY(socket)
if test "$ac_cv_lib_socket_main" = yes ; then
    AC_HAVE_LIBRARY(nsl)
fi

# this one is for Tru64 and bind_to_cpu_id
AC_HAVE_LIBRARY(mach)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
# AC_TYPE_SOCKLEN_T

OLD_TYPE_SOCKLEN_T

# AC_TYPE_IN_PORT_T
AC_DECL_H_ERRNO
AC_STRUCT_SOCKADDR_STORAGE
AC_HEADER_TIME
AC_HEADER_STDBOOL

# Checks for library functions.
# AC_FUNC_ERROR_AT_LINE
# AC_FUNC_FORK
# AC_FUNC_MALLOC
# AC_FUNC_MMAP
AC_FUNC_SELECT_ARGTYPES
# AC_FUNC_SETPGRP
# AC_TYPE_SIGNAL
# AC_FUNC_STAT
# remove pstat_getdynamic (at least for now) since we don't do
# anything conditional with the check anyway...
AC_CHECK_FUNCS([gethrtime gettimeofday inet_ntoa memset memcpy socket \
		sqrt strcasecmp strchr strstr strtoul uname])

#AC_CONFIG_SUBDIRS(src/missing)

# does this platform need the replacement getaddrinfo or other
AC_CHECK_FUNCS([getnameinfo getaddrinfo inet_ntop])
# AC_REPLACE_FUNCS([getaddrinfo])


if test "$ac_cv_func_getaddrinfo$ac_cv_func_getnameinfo" != yesyes ; then
   AC_MSG_NOTICE([Requesting replacement getaddrinfo/getnameinfo])
   AC_LIBOBJ(missing/getaddrinfo)
fi
if test "$ac_cv_func_inet_ntop" != yes ; then
   AC_MSG_NOTICE([Requesting replacement inet_ntop])
   AC_LIBOBJ(missing/inet_ntop)
fi

AC_CHECK_FUNCS(sendfile)

AC_CHECK_FUNCS(bindprocessor bind_to_cpu_id mpctl processor_bind sched_setaffinity)

# since we are going to depend on pkg-config for glib, might as well
# use it for libxml2 as well

#AC_CHECK_PROG(have_libxml2, xml2-config, "yes", "no" )
#if test "x$have_libxml2"="xyes"; then
#	libxml2_cflags=`xml2-config --cflags`
#	libxml2_libs=`xml2-config --libs`
#else
#	AC_MSG_ERROR(Cannot find xml2-config.)
#fi

AC_ARG_WITH(glib,
	[AS_HELP_STRING([--with-glib],
	[in the future will show where/how to include glib])])

pkg_config_cflags=""
pkg_config_libs_l=""
pkg_config_libs_L=""
pkg_config_other_cflags=""
pkg_config_other_libs=""

    PKG_PROG_PKG_CONFIG()

    pkg_config_cflags=`$PKG_CONFIG --cflags-only-I \
			libxml-2.0 glib-2.0 gmodule-2.0 gthread-2.0`
    pkg_config_other_cflags=`$PKG_CONFIG --cflags-only-other \
			libxml-2.0 glib-2.0 gmodule-2.0 gthread-2.0`
    pkg_config_libs_L=`$PKG_CONFIG  --libs-only-L \
			libxml-2.0 glib-2.0 gmodule-2.0  gthread-2.0`
    pkg_config_libs_l=`$PKG_CONFIG  --libs-only-l \
			libxml-2.0 glib-2.0 gmodule-2.0  gthread-2.0`
    pkg_config_other_libs=`$PKG_CONFIG  --libs-only-other \
			libxml-2.0 glib-2.0 gmodule-2.0  gthread-2.0`

echo
echo "OTHER CFLAGS" $pkg_config_other_cflags
echo "OTHER LIBS" $pkg_config_other_libs
echo

# see if we should be enabling histogram support

AC_MSG_CHECKING(whether to include histogram support)

AC_ARG_ENABLE(histogram,
	[AS_HELP_STRING([--enable-histogram],
	[include individual op timing, may affect result]) ])

case "$enable_histogram" in
	yes)
		use_histogram=true
		;;
	no)
		use_histogram=false
		;;
	'')
		# whatever
		use_histogram=false
		;;
	*)
		AC_MSG_ERROR([--enable-histogram takes yes or no])
		;;
esac

if $use_histogram
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

if $use_histogram
then
	AC_DEFINE([WANT_HISTOGRAM],,
	[Define to one to enable histogram support. May affect results.])
fi

# see if we should be enabling dns support

AC_MSG_CHECKING(whether to include dns support)

AC_ARG_ENABLE(dns,
	[AS_HELP_STRING([--enable-dns],
	[build the DNS performance test suite]) ])

case "$enable_dns" in
	yes)
		use_dns=true
		;;
	no)
		use_dns=false
		;;
	'')
		# whatever
		use_dns=false
		;;
	*)
		AC_MSG_ERROR([--enable-dns takes yes or no])
		;;
esac

if $use_dns
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

AM_CONDITIONAL([WANT_DNS], [test "$use_dns" = true])

# see if we should be enabling dns support

AC_MSG_CHECKING(whether to include disk support)

AC_ARG_ENABLE(disk,
	[AS_HELP_STRING([--enable-disk],
	[build the DISK performance test suite]) ])

case "$enable_disk" in
	yes)
		use_disk=true
		;;
	no)
		use_disk=false
		;;
	'')
		# whatever
		use_disk=false
		;;
	*)
		AC_MSG_ERROR([--enable-disk takes yes or no])
		;;
esac

if $use_disk
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

AM_CONDITIONAL([WANT_DISK], [test "$use_disk" = true])

# see if we should be enabling vst support

AC_MSG_CHECKING(whether to include vst support)

AC_ARG_ENABLE(vst,
	[AS_HELP_STRING([--enable-vst],
	[build the VST performance test suite]) ])

case "$enable_vst" in
	yes)
		use_vst=true
		;;
	no)
		use_vst=false
		;;
	'')
		# whatever
		use_vst=false
		;;
	*)
		AC_MSG_ERROR([--enable-vst takes yes or no])
		;;
esac

if $use_vst
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

AM_CONDITIONAL([WANT_VST], [test "$use_vst" = true])

# see if we should be enabling demo support

AC_MSG_CHECKING(whether to include demo support)

AC_ARG_ENABLE(demo,
	[  --enable-demo   emit interim results during the run. May affect results.])

case "$enable_demo" in
	yes)
		use_demo=true
		;;
	no)
		use_demo=false
		;;
	'')
		# whatever
		use_demo=false
		;;
	*)
		AC_MSG_ERROR([--enable-demo takes yes or no])
		;;
esac

if $use_demo
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

if $use_demo
then
	AC_DEFINE([WANT_DEMO],,
		  [Define to one to enable demo support. May affect results.])
fi

# see if we should be including the AF_UNIX tests

AC_MSG_CHECKING(whether to include Unix-domain socket tests)

AC_ARG_ENABLE(unixdomain,
	[  --enable-unixdomain  include Unix Domain socket tests])

case "$enable_unixdomain" in
     yes)
		use_unixdomain=true
		;;
     no)	
		use_unixdomain=false
		;;
     '')
		use_unixdomain=false
		;;
     *)
		AC_MSG_ERROR([--enable-unixdomain takes yes or no])
		;;
esac

if $use_unixdomain
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

if $use_unixdomain
then
	AC_DEFINE([WANT_UNIX],,
		  [Define to one to include Unix Domain socket tests.])
fi

# see if we should be including the DLPI tests

AC_MSG_CHECKING(whether to include DLPI tests)

AC_ARG_ENABLE(dlpi,
	[  --enable-dlpi  include Unix Domain socket tests])

case "$enable_dlpi" in
     yes)
		use_dlpi=true
		;;
     no)	
		use_dlpi=false
		;;
     '')
		use_dlpi=false
		;;
     *)
		AC_MSG_ERROR([--enable-dlpi takes yes or no])
		;;
esac

if $use_dlpi
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

if $use_dlpi
then
	AC_DEFINE([WANT_DLPI],,[Define to one to include DLPI tests.])
fi


# see if we should be including the XTI tests

AC_MSG_CHECKING(whether to include XTI tests)

AC_ARG_ENABLE(xti,
	[  --enable-xti  include Unix Domain socket tests])

case "$enable_xti" in
     yes)
		# probably need to be a bit more sophisticated here
		AC_CHECK_LIB(xti,t_open)
		use_xti=true
		;;
     no)	
		use_xti=false
		;;
     '')
		use_xti=false
		;;
     *)
		AC_MSG_ERROR([--enable-xti takes yes or no])
		;;
esac

if $use_xti
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

if $use_xti
then
	AC_DEFINE([WANT_XTI],,[Define to one to include XTI tests.])
fi

# see if we should be including the ICSC-EXS tests

AC_MSG_CHECKING(whether to include ICSC-EXS tests)

AC_ARG_ENABLE(exs,
	[  --enable-exs  include ICSC async sockets tests])

case "$enable_exs" in
     yes)
		use_exs=true
		AC_CHECK_HEADER(sys/exs.h,,[use_exs=false])
		AC_CHECK_LIB(exs,exs_init,,[use_exs=false])
		;;
     no)	
		use_exs=false
		;;
     '')
		use_exs=false
		;;
     *)
		AC_MSG_ERROR([--enable-exs takes yes or no])
		;;
esac

if $use_exs
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

if $use_exs
then
	AC_DEFINE([HAVE_ICSC_EXS],,[Define to one to include ICSC-EXS tests.])
fi

# see if we should be enabling SCTP support


AC_ARG_ENABLE(sctp,
	[  --enable-sctp   include tests to measure SCTP performance ])

case "$enable_sctp" in
	yes)
		use_sctp=true
		AC_CHECK_HEADERS(netinet/sctp.h,,use_sctp=false,
[[
#include <sys/socket.h>
]])
		AC_HAVE_LIBRARY(sctp,,use_sctp=false)
		;;
	no)
		use_sctp=false
		;;
	'')
		# whatever
		use_sctp=false
		;;
	*)
		AC_MSG_ERROR([--enable-sctp takes yes or no])
		;;
esac

AC_MSG_CHECKING(whether to include SCTP tests)

if $use_sctp
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

if $use_sctp
then
	AC_DEFINE([WANT_SCTP],,[Define to one to include SCTP tests.])
fi

# see if we should be enabling paced sends

AC_MSG_CHECKING([whether to include paced send (intervals) support])

AC_ARG_ENABLE(intervals,
	[  --enable-intervals  include ability to pace operations, may affect result])

case "$enable_intervals" in
	yes)
		use_intervals=true
		;;
	no)
		use_intervals=false
		;;
	'')
		use_intervals=false
		;;
	*)
		AC_MSG_ERROR([--enable-intervals takes yes or no])
		;;
esac

if $use_intervals
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

if $use_intervals
then
	AC_DEFINE([WANT_INTERVALS],,
	[Define to one to enable paced operation support. May affect results.])
fi

# see if we should be enabling initial request bursts

AC_MSG_CHECKING([whether to include initial burst support in _RR tests])

AC_ARG_ENABLE(burst,
	[  --enable-burst  include intial request burst ability in _RR tests, may affect result])

case "$enable_burst" in
	yes)
		use_burst=true
		;;
	no)
		use_burst=false
		;;
	'')
		use_burst=false
		;;
	*)
		AC_MSG_ERROR([--enable-burst takes yes or no])
		;;
esac

if $use_burst
then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

if $use_burst
then
	AC_DEFINE([WANT_FIRST_BURST],,
[Define to one to enable initial _RR burst support. May affect results.])
fi

# time to see about CPU utilization measurements

AC_MSG_CHECKING([which CPU utilization measurement type to use])

AC_ARG_ENABLE(cpuutil,
	[  --enable-cpuutil  include code to measure CPU utilization using specified mechanism])

NETSYS_SOURCE="$enable_cpuutil"
case "$enable_cpuutil" in
	pstat)
		use_cpuutil=true
		AC_DEFINE([USE_PSTAT],,
			[Use HP-UX's pstat interface to measure CPU util.])
		;;
	pstatnew)
		use_cpuutil=true
		AC_DEFINE([USE_PSTAT],,
			[Use HP-UX's pstat interface to measure CPU util.])
		;;
        perfstat)
                use_cpuutil=true
                AC_DEFINE([USE_PERFSTAT],,
			[Use AIX's perfstat interface to measure CPU util.])
		AC_HAVE_LIBRARY(perfstat)
                ;;

	looper)
		use_cpuutil=true
		AC_DEFINE([USE_LOOPER],,
			[Use looper/soaker processes to measure CPU util.])
		;;		
	procstat)
		use_cpuutil=true
		AC_DEFINE([USE_PROC_STAT],,
			[Use Linux's procstat interface to measure CPU util.])
		;;
	kstat)
		use_cpuutil=true
		AC_DEFINE([USE_KSTAT],,
			[Use Solaris's kstat interface to measure CPU util.])
		AC_HAVE_LIBRARY(kstat)
		;;
	kstat10)
		use_cpuutil=true
		AC_DEFINE([USE_KSTAT],,
			[Use Solaris's kstat interface to measure CPU util.])
		AC_HAVE_LIBRARY(kstat)
		;;
	'')
# ia64-hp-hpux11.23
# i386-pc-solaris2.10
# guess it automagically in a nice big case statement
		case $target in
		   *-*-linux*)	
		     use_cpuutil=true
		     AC_DEFINE([USE_PROC_STAT],,
		      [Use Linux's procstat interface to measure CPU util.])
		      enable_cpuutil="procstat - auto"
		      NETSYS_SOURCE="linux"
		      ;;
		   *-*-hpux11.23 | *-*-hpux11.31)
		     use_cpuutil=true
		     AC_DEFINE([USE_PSTAT],,
		     [Use HP-UX's pstat interface to measure CPU util.])
		     enable_cpuutil="pstatnew - auto"
		     NETSYS_SOURCE="hpux"
		     ;;
		   *-*-hpux11* | *-*-hpux10*)
		     use_cpuutil=true
		     AC_DEFINE([USE_PSTAT],,
		     [Use HP-UX's pstat interface to measure CPU util.])
		     enable_cpuutil="pstat - auto"
		     NETSYS_SOURCE="pstat"
		     ;;
		   *-*-aix5.*)
		     use_cpuutil=true
		     AC_DEFINE([USE_PERFSTAT],,
		     [Use AIX's perfstat interface to measure CPU util.])
		     AC_HAVE_LIBRARY(perfstat)
		     enable_cpuutil="perfstat - auto"
		     NETSYS_SOURCE="aix"
		     ;;
		   *-*-solaris2.10 | *-*-solaris2.11)
		     use_cpuutil=true
		     AC_DEFINE([USE_KSTAT],,
		     [Use Solaris's kstat interface to measure CPU util.])
		     AC_HAVE_LIBRARY(kstat)
		     enable_cpuutil="kstat10 - auto"
		     NETSYS_SOURCE="kstat10"
		     ;;
		   *-*-solaris2.*)
		     use_cpuutil=true
		     AC_DEFINE([USE_KSTAT],,
		     [Use Solaris's kstat interface to measure CPU util.])
		     AC_HAVE_LIBRARY(kstat)
		     enable_cpuutil="kstat - auto"
		     NETSYS_SOURCE="kstat"
		     ;;
                   *-*-freebsd4.* | *-*-freebsd5.* | *-*-freebsd6.*)
		     use_cpuutil=true
		     AC_DEFINE([USE_SYSCTL],,
		     [Use FreeBSD's sysctl interface to measure CPU util.])
		     enable_cpuutil="sysctl - auto"
		     NETSYS_SOURCE="sysctl"
		     ;;
		   *)
		     use_cpuutil=false
		     NETSYS_SOURCE="none"
		     enable_cpuutil="none. Consider teaching configure about your platform."
		     ;;
		esac
		;;
	none)
		use_cpuutil=false
		;;
	*)
		AC_MSG_ERROR(--enable-cpuutil takes other values)
		;;
esac

	AC_MSG_RESULT("$enable_cpuutil")

# if the compiler is gcc, we need/want -pthread - at least I think we
# do for multi-threaded compiles.  we also do things this way so we can
# use the "only" options in pkg-config to we don't end-up trying to pass
# gcc options to other compilers if the dependencies were built with
# gcc and we are not being built with gcc - thusfar the only option that
# has appeared that way is the "-pthread" option.  i'm sure that as we
# find more of them we'll start doing more "interesting" things in the
# name of not forcing a specific compiler on people, because:
#
# PORTABLE CODE DOES NOT DEPEND ON A SPECIFIC COMPILER
#


case $GCC in
     yes)
         CFLAGS="$CFLAGS -pthread $pkg_config_cflags"
	 LIBS="$LIBS -Wl,--export-dynamic -pthread $pkg_config_libs_L $pkg_config_libs_l"
	 ;;
       *)
         CFLAGS="$CLFAGS $pkg_config_cflags "
	 LIBS="$LIBS $pkg_config_libs_L $pkg_config_libs_l "
	 ;;
esac

AC_SUBST(CFLAGS)
AC_SUBST(LIBS)

AC_SUBST(NETSYS_SOURCE)
AC_SUBST(LIBOJBS)

# now spit it all out
AC_CONFIG_FILES([Makefile
                 include/Makefile
                 src/Makefile
		 doc/Makefile
		 suites/Makefile
		 suites/dns/Makefile
		 suites/disk/Makefile
		 suites/vst/Makefile
		 netperf-config])

AC_OUTPUT
