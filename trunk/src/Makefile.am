#SUBDIRS = missing

bin_PROGRAMS = netperf netserver

AM_CFLAGS = $(NETPERF_CFLAGS)

USE_CPU_SOURCE=netsysstats_@NETSYS_SOURCE@.c

EXTRA_DIST = missing/* dirs NetPerf/* NetServer/* *.xml netperf_docs.dtd netperf.h netconfidence.h netsysstats*.c 

COMMON_SRC = hist.h netlib.c netperf.h netlib.h netcpu.h netmsg.c nettest_bsd.c nettest_bsd.h netconfidence.c

netperf_SOURCES = netperf.c $(COMMON_SRC) $(USE_CPU_SOURCE)
netserver_SOURCES = netserver.c $(COMMON_SRC) $(USE_CPU_SOURCE)

# if there are any "missing" routines, the libobjs should cover it
netperf_LDADD = @LIBOBJS@
netserver_LDADD = @LIBOBJS@

missing/getaddrinfo.o: $(srcdir)/missing/getaddrinfo.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/getaddrinfo.c
missing/inet_ntop.o: $(srcdir)/missing/inet_ntop.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/inet_ntop.c
missing/getopt1.o: $(srcdir)/missing/getopt1.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/getopt1.c
missing/getopt.o: $(srcdir)/missing/getopt.c
	$(COMPILE) -o $@ -c $(srcdir)/missing/getopt.c
