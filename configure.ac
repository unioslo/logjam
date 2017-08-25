AC_PREREQ([2.63])
AC_INIT([logjam], [0.20170404])
AC_CONFIG_SRCDIR([include/logjam/logjam.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([include/config.h])

# C compiler and features
AC_LANG(C)
AC_PROG_CC([clang gcc cc])
AC_PROG_CC_STDC
AC_PROG_CPP
AC_PROG_CXX([clang++ g++ c++])
AC_GNU_SOURCE
AC_C_CONST
AC_C_RESTRICT
AC_C_VOLATILE
AC_C_BIGENDIAN
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT8_T
AC_TYPE_INTMAX_T
AC_TYPE_INTPTR_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_TYPE_UINTMAX_T
AC_TYPE_UINTPTR_T

# other programs
LT_PREREQ([2.2.6])
LT_INIT()
AC_PROG_INSTALL
AX_PROG_PKG_CONFIG

# misc headers and functions
AC_CHECK_FUNCS([strchrnul strlcat strlcpy])

# systemd
AC_ARG_ENABLE([systemd],
    AS_HELP_STRING([--enable-systemd], [enable systemd support (default is NO)]),
    [enable_systemd=$enableval], [enable_systemd=no])
AX_PKG_CONFIG_CHECK([libsystemd])

# threads
AX_PTHREAD([], [
    AC_MSG_ERROR([${PACKAGE_NAME} requires a POSIX thread library.])
])

# json
AX_PKG_CONFIG_REQUIRE([jansson])

# ssl
AX_PKG_CONFIG_CHECK([gnutls], [], [
    AC_MSG_WARN([GnuTLS not found, network encryption disabled.])
])

# default configuration file
_LJ_DEFAULT_CONFIG='${sysconfdir}/${PACKAGE_NAME}.conf'
AC_ARG_WITH([default-config],
    AS_HELP_STRING([--with-default-config=path], [default configuration file]),
    [lj_default_config=withval], [lj_default_config=${_LJ_DEFAULT_CONFIG}])
AC_SUBST([LJ_DEFAULT_CONFIG], [$lj_default_config])

# cryb-test
AX_PKG_CONFIG_CHECK([cryb-test],
  [AC_MSG_NOTICE([Cryb test framework found, unit tests enabled.])],
  [AC_MSG_WARN([Cryb test framework not found, unit tests disabled.])])

# debugging options
AC_ARG_ENABLE([developer-warnings],
    AS_HELP_STRING([--enable-developer-warnings], [enable strict warnings (default is NO)]),
    [CFLAGS="${CFLAGS} -Wall -Wextra"])
AC_ARG_ENABLE([debugging-symbols],
    AS_HELP_STRING([--enable-debugging-symbols], [enable debugging symbols (default is NO)]),
    [CFLAGS="${CFLAGS} -O0 -g -fno-inline"])
AC_ARG_ENABLE([werror],
    AS_HELP_STRING([--enable-werror], [use -Werror (default is NO)]),
    [CFLAGS="${CFLAGS} -Werror"])

AC_CONFIG_FILES([
    Makefile
    include/Makefile
    lib/Makefile
    lib/logjam/Makefile
    sbin/Makefile
    sbin/logjam/Makefile
    t/Makefile
])
AC_OUTPUT
