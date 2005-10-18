EXTRA_DIST = netperf4.man netserver4.man netperf4.txt netperf4.html netperf4.xml  netperf4.pdf netperf4.ps netperf4.texi examples netperf4_design.txt
man1_MANS = netperf4.man netserver4.man
info_TEXINFOS = netperf4.texi
CLEANFILES = netperf4.txt netperf4.xml netperf4.html

netperf4.html: $(info_TEXINFOS)
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) --html --no-split -I $(srcdir) \
         -o $@ `test -f '$<' || echo '$(srcdir)/'`$<

netperf4.txt: $(info_TEXINFOS)
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) --plaintext -I $(srcdir) \
         -o $@ `test -f '$<' || echo '$(srcdir)/'`$<

netperf4.xml: $(info_TEXINFOS)
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) --xml -I $(srcdir) \
         -o $@ `test -f '$<' || echo '$(srcdir)/'`$<
