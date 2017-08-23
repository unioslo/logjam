AM_CPPFLAGS = -I$(top_srcdir)/include

EXTRA_DIST =

if HAVE_CRYB_TEST

liblogjam = $(top_builddir)/lib/logjam/liblogjam.la

TESTS =

TESTS += t_cirq t_strchrnul t_strlcat t_strlcpy
t_cirq_CFLAGS = $(CRYB_TEST_CFLAGS) $(PTHREAD_CFLAGS)
t_cirq_LDADD = $(liblogjam) $(CRYB_TEST_LIBS) $(PTHREAD_LIBS)
t_strchrnul_CFLAGS = $(CRYB_TEST_CFLAGS)
t_strchrnul_LDADD = $(liblogjam) $(CRYB_TEST_LIBS)
t_strlcat_CFLAGS = $(CRYB_TEST_CFLAGS)
t_strlcat_LDADD = $(liblogjam) $(CRYB_TEST_LIBS)
t_strlcpy_CFLAGS = $(CRYB_TEST_CFLAGS)
t_strlcpy_LDADD = $(liblogjam) $(CRYB_TEST_LIBS)

check_PROGRAMS = $(TESTS)

endif
