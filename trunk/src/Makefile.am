#SUBDIRS = missing

bin_PROGRAMS = netperf netserver

AM_CFLAGS = $(NETPERF_CFLAGS)

USE_CPU_SOURCE=netsysstats_@NETSYS_SOURCE@.c

# anything that might be platform-specific and thus not covered
# directly in COMMON_SRC or *_SOURCES should be in EXTRA_DIST

EXTRA_DIST = missing/* dirs NetPerf/* NetServer/* *.xml netperf_docs.dtd netperf.h netsysstats*.c 

COMMON_SRC = hist.h netlib.c netperf.h netlib.h netcpu.h netmsg.c netconfidence.c netconfidence.h

netperf_SOURCES = netperf.c $(COMMON_SRC)
netserver_SOURCES = netserver.c $(COMMON_SRC)

# in theory this will cause these header files to be put in the
# installed headers location, where folks who want to make their own
# test libraries can find them
include_HEADERS = netperf.h netconfidence.h

# in theory, the stuff below should deal with creating the requisite libs
lib_LTLIBRARIES = nettest_bsd.la netsysstats.la nettest_dns.la

nettest_bsd_la_SOURCES = nettest_bsd.c nettest_bsd.h
nettest_bsd_la_LDFLAGS = -module

nettest_dns_la_SOURCES = nettest_dns.c nettest_dns.h
nettest_dns_la_LDFLAGS = -module

# the USE_CPU_SOURCE will pick the apropriate file based on the platform
netsysstats_la_SOURCES = netsysstats_common.c netsysstats.h $(USE_CPU_SOURCE)
netsysstats_la_LDFLAGS = -module

# if there are any "missing" routines, the libobjs should cover it
netperf_LDADD = @LIBOBJS@
netserver_LDADD = @LIBOBJS@

# so far, it does not appear we need this in netserver
netperf_LDFLAGS = -export-dynamic


missing/getaddrinfo.o: $(srcdir)/missing/getaddrinfo.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/getaddrinfo.c
missing/inet_ntop.o: $(srcdir)/missing/inet_ntop.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/inet_ntop.c
missing/getopt1.o: $(srcdir)/missing/getopt1.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/getopt1.c
missing/getopt.o: $(srcdir)/missing/getopt.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/getopt.c
