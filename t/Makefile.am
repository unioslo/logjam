AM_CPPFLAGS = -I$(top_srcdir)/include

EXTRA_DIST =

if HAVE_CRYB_TEST

liblogjam = $(top_builddir)/lib/logjam/liblogjam.la

TESTS =

TESTS += t_cirq
t_cirq_CPPFLAGS = $(CRYB_TEST_CFLAGS) $(PTHREAD_CFLAGS)
t_cirq_LDADD = $(liblogjam) $(CRYB_TEST_LIBS) $(PTHREAD_LIBS)

check_PROGRAMS = $(TESTS)

endif
