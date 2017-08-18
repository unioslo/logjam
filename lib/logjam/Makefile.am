AM_CPPFLAGS		 = -I$(top_srcdir)/include

lib_LTLIBRARIES		 = liblogjam.la

liblogjam_la_SOURCES	 = \
	cirq.c \
	connect.c \
	resolve.c \
	strchrnul.c \
	strlcat.c \
	strlcpy.c
liblogjam_la_CPPFLAGS	 = $(PTHREAD_CFLAGS)
liblogjam_la_LIBADD	 = $(PTHREAD_LIBS)
