EXTRA_DIST = \
	logjam.systemd.in

GENFILES = logjam.systemd
CLEANFILES = $(GENFILES) edit-stamp
noinst_DATA = edit-stamp

edit-stamp: $(GENFILES) Makefile
	$(SED) -i \
	    -e "s@\$${prefix}@${prefix}@" \
	    -e "s@\$${exec_prefix}@${exec_prefix}@" \
	    $(GENFILES)
	touch edit-stamp
