AM_CPPFLAGS		 = -I$(top_srcdir)/include

lib_LTLIBRARIES		 = liblogjam.la

liblogjam_la_SOURCES	 = \
	cirq.c \
	connect.c \
	resolve.c \
	socket.c \
	strchrnul.c \
	strlcat.c \
	strlcpy.c
liblogjam_la_CFLAGS	 = $(GNUTLS_CFLAGS) $(PTHREAD_CFLAGS)
liblogjam_la_LIBADD	 = $(GNUTLS_LIBS) $(PTHREAD_LIBS)
