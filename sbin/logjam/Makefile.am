AM_CPPFLAGS	 = -I$(top_srcdir)/include

sbin_PROGRAMS	 = logjam

logjam_SOURCES	 = logjam.c logobj.c main.c
logjam_CFLAGS	 = $(JANSSON_FLAGS) $(PTHREAD_CFLAGS)
logjam_LDADD	 = $(top_builddir)/lib/logjam/liblogjam.la $(JANSSON_LIBS) $(PTHREAD_LIBS)

# BIND log parser
logjam_SOURCES	+= bind.c

# ELK submitter
logjam_SOURCES	+= elk.c

# Systemd log source
if HAVE_LIBSYSTEMD
logjam_SOURCES	+= systemd.c
logjam_CFLAGS	+= $(LIBSYSTEMD_CFLAGS)
logjam_LDADD	+= $(LIBSYSTEMD_LIBS)
endif HAVE_LIBSYSTEMD

dist_man1_MANS = logjam.8