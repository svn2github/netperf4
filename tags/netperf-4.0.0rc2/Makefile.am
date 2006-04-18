SUBDIRS = src doc
EXTRA_DIST =  m4/ README.* Release_Notes inet_ntop.c netperf-config.in

bin_SCRIPTS = netperf-config

dist-hook:
	rm -rf `find $(distdir) -name .svn`
	rm -rf `find $(distdir) -name "*~"`