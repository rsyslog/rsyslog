#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([librelp], [1.2.7], [rgerhards@adiscon.com])
AM_INIT_AUTOMAKE
AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_SRCDIR([src/relp.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
if test "$GCC" = "yes"
then CFLAGS="$CFLAGS -W -Wall -Wformat-security -Wshadow -Wcast-align -Wpointer-arith -Wmissing-format-attribute -g"
fi

AC_PROG_LIBTOOL

# Checks for libraries.
save_LIBS=$LIBS
LIBS=
AC_SEARCH_LIBS(clock_gettime, rt)
rt_libs=$LIBS
LIBS=$save_LIBS

AC_SUBST(rt_libs)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([sys/epoll.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
#AC_HEADER_TIME
#AC_STRUCT_TM

AC_CHECK_MEMBERS([struct sockaddr.sa_len],,,[$sa_includes])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STRERROR_R
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([strerror_r epoll_create epoll_create1])

# enable TLS (may not be possible on platforms with too-old GnuTLS)
AC_ARG_ENABLE(tls,
        [AS_HELP_STRING([--enable-tls],[Enable TLS support @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_tls="yes" ;;
          no) enable_tls="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-tls) ;;
         esac],
        [enable_tls="yes"]
)
if test "$enable_tls" = "yes"; then
	PKG_CHECK_MODULES(GNUTLS, gnutls >= 2.0.0)
        AC_DEFINE(ENABLE_TLS, 1, [Defined if TLS support is enabled])
	# Check if we have support for proper cert validation
	AC_MSG_CHECKING(if we have gnutls_certificate_set_verify_function)
	save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $GNUTLS_CFLAGS"
	save_LIBS="$LIBS"
	LIBS="$LIBS $GNUTLS_LIBS"
	AC_TRY_LINK(
		[
		#include <gnutls/gnutls.h>
		#include <gnutls/x509.h>
		], [
		gnutls_certificate_set_verify_function(NULL, NULL);
		],[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_GNUTLS_CERTIFICATE_SET_VERIFY_FUNCTION, 1, [do we have gnutls_certificate_set_verify_function])
		have_gnutls_certificate_set_verify_function=yes
		],[
		AC_MSG_RESULT(no; authentication disabled)
		have_gnutls_certificate_set_verify_function=no
		]
	)
	CFLAGS="$save_CFLAGS"
	LIBS="$save_LIBS"
fi


# debug mode settings
AC_ARG_ENABLE(debug,
        [AS_HELP_STRING([--enable-debug],[Enable debug mode @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_debug="yes" ;;
          no) enable_debug="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
         esac],
        [enable_debug="no"]
)
if test "$enable_debug" = "yes"; then
        AC_DEFINE(DEBUG, 1, [Defined if debug mode is enabled (it's easier to check in the code).])
fi
if test "$enable_debug" = "no"; then
        AC_DEFINE(NDEBUG, 1, [Defined if debug mode is disabled.])
fi



AC_CONFIG_FILES([Makefile \
		relp.pc \
		doc/Makefile \
		src/Makefile])
AC_OUTPUT


echo "*****************************************************"
echo "librelp will be compiled with the following settings:"
echo
echo "Debug mode enabled:            $enable_debug"
echo "TLS enabled:                   $enable_tls"
echo "TLS authentication supported:  $have_gnutls_certificate_set_verify_function"

