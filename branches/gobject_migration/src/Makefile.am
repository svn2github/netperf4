#SUBDIRS = suites

#bin_PROGRAMS = netperf4 netserver
bin_PROGRAMS = netperf4

AM_CFLAGS = $(NETPERF_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/include -DLIBDIR="\"$(libdir)\"" -DNETPERFDIR="\"$(netperfdir)\""

# for now we will use "NETSYS_SOURCE" for both the netsysstats and
# netlib platform-specific stuff
USE_CPU_SOURCE=netsysstats_@NETSYS_SOURCE@.c
USE_NETLIB_SOURCE=netlib_@NETSYS_SOURCE@.c

# anything that might be platform-specific and thus not covered
# directly in COMMON_SRC or *_SOURCES should be in EXTRA_DIST

EXTRA_DIST = missing/* dirs NetPerfDir/* NetServerDir/* *.xml netperf_docs.dtd netsysstats*.c netlib_*.c

# some of the other include files are actually up in include/
COMMON_SRC = netlib.c netcpu.h netmsg.c netmsg.h netconfidence.c
FOO_SRC = netperf-netserver.c netperf-netserver.h netperf-control.h netperf-control.c netperf-test.c netperf-test.h netlib4.c

netperf4_SOURCES = netperf4.c $(FOO_SRC) $(USE_NETLIB_SOURCE)
#netserver_SOURCES = netserver.c $(COMMON_SRC) $(USE_NETLIB_SOURCE)

netperf4dir = @datadir@/netperf
netperf4_DATA = netperf_docs.dtd default_commands.xml default_config.xml

# in theory, the stuff below should deal with creating the requisite
# "core" libs.  optional libs are in suites/
lib_LTLIBRARIES = nettest_bsd.la netsysstats.la

nettest_bsd_la_SOURCES = nettest_bsd.c nettest_bsd.h
nettest_bsd_la_LDFLAGS = -module

# the USE_CPU_SOURCE will pick the apropriate file based on the platform
netsysstats_la_SOURCES = netsysstats_common.c netsysstats.h $(USE_CPU_SOURCE)
netsysstats_la_LDFLAGS = -module

# if there are any "missing" routines, the libobjs should cover it
netperf4_LDADD = @LIBOBJS@
#netserver_LDADD = @LIBOBJS@

# so far, it does not appear we need this in netserver
netperf4_LDFLAGS = -rpath $(libdir) -export-dynamic
#netserver_LDFLAGS = -rpath $(libdir) -export-dynamic

missing/getaddrinfo.o: $(srcdir)/missing/getaddrinfo.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/getaddrinfo.c
missing/inet_ntop.o: $(srcdir)/missing/inet_ntop.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/inet_ntop.c
missing/getopt1.o: $(srcdir)/missing/getopt1.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/getopt1.c
missing/getopt.o: $(srcdir)/missing/getopt.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/getopt.c

# a boatload of assumptions here, but this should create the /etc/xml/catalog
# with our entry in it. we really should provide some overrides in the 
# configure script one of these days
install-data-local:
	if [ ! -d /etc/xml ]; then mkdir /etc/xml; chmod 555 /etc/xml; fi
	if [ ! -f /etc/xml/catalog ]; then xmlcatalog --create --noout /etc/xml/catalog;fi
	xmlcatalog --noout --add "system" "http://www.netperf.org/netperf_docs.dtd/1.0" "file://$(netperf4dir)/netperf_docs.dtd" /etc/xml/catalog

