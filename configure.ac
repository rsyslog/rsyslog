#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([rsyslog],[1.21.1],[rsyslog@lists.adiscon.com.])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([syslogd.c])
AC_CONFIG_HEADERS([config.h])

AC_GNU_SOURCE

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
if test "$GCC" = "yes"
then CFLAGS="$CFLAGS -W -Wall -Wformat-security -Wshadow -Wcast-align -Wpointer-arith -Wmissing-format-attribute -g"
fi
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_CANONICAL_HOST

case "${host}" in
  *-*-linux*)
    # This feature indicates if klogd functionality
    # should be integrated. If it is switched off, klogd
    # is still compiled, but it is an empty shell.
    AC_DEFINE([FEATURE_KLOGD], [1], [Description])
  ;;
  *-*-*darwin*|*-*-freebsd*|*-*-netbsd*|*-*-openbsd*)
    AC_DEFINE([BSD], [1], [Description])
  ;;
esac

# Checks for libraries.
save_LIBS=$LIBS
LIBS=
AC_SEARCH_LIBS(clock_gettime, rt)
rt_libs=$LIBS
LIBS=
AC_SEARCH_LIBS(dlopen, dl)
dl_libs=$LIBS
LIBS=$save_LIBS

AC_SUBST(rt_libs)
AC_SUBST(dl_libs)

# Checks for header files.
AC_HEADER_RESOLV
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h locale.h netdb.h netinet/in.h paths.h stddef.h stdlib.h string.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h syslog.h unistd.h utmp.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_MODE_T
AC_TYPE_UID_T
AC_TYPE_UINT8_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

sa_includes="\
$ac_includes_default
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
"
AC_CHECK_MEMBERS([struct sockaddr.sa_len],,,[$sa_includes])

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_FUNC_WAIT3
AC_CHECK_FUNCS([alarm clock_gettime gethostbyname gethostname gettimeofday localtime_r memset mkdir regcomp select setid socket strcasecmp strchr strdup strerror strerror_r strndup strnlen strrchr strstr strtol strtoul uname ttyname_r])


# Large file support
AC_ARG_ENABLE(largefile,
        [AS_HELP_STRING([--enable-largefile],[Enable large file support @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_largefile="yes" ;;
          no) enable_largefile="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-largefile) ;;
         esac],
        [enable_largefile="yes"]
)
if test "$enable_largefile" = "no"; then
        AC_DEFINE(NOLARGEFILE, 1, [Defined when large file support is disabled.])
fi

# Regular expressions
AC_ARG_ENABLE(regexp,
        [AS_HELP_STRING([--enable-regexp],[Enable regular expressions support @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_regexp="yes" ;;
          no) enable_regexp="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-regexp) ;;
         esac],
        [enable_regexp=yes]
)
if test "$enable_regexp" = "yes"; then
        AC_DEFINE(FEATURE_REGEXP, 1, [Regular expressions support enabled.])
fi

# zlib compression
AC_ARG_ENABLE(zlib,
        [AS_HELP_STRING([--enable-zlib],[Enable zlib compression  support @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_zlib="yes" ;;
          no) enable_zlib="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-zlib) ;;
         esac],
        [enable_zlib=yes]
)
if test "$enable_zlib" = "yes"; then
	AC_CHECK_HEADER(zlib.h, [zlib_header="yes"], [zlib_header="no" enable_zlib="false"])
	if test "$zlib_header" = "yes"; then
		AC_CHECK_LIB(z, deflate,
                        [AC_DEFINE(USE_NETZIP, 1, [Define if you want to enable zlib support]) 
                         zlib_libs="-lz"
                         AC_SUBST(zlib_libs)], enable_zlib="false")
	fi
fi

#gssapi
AC_ARG_ENABLE(gssapi_krb5,
	[AS_HELP_STRING([--enable-gssapi-krb5],[Enable GSSAPI Kerberos 5 support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) want_gssapi_krb5="yes" ;;
          no) want_gssapi_krb5="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-gssapi-krb5) ;;
         esac],
	[want_gssapi_krb5=no]
)
if test $want_gssapi_krb5 = yes; then
	AC_CHECK_LIB(gssapi_krb5, gss_acquire_cred, [
		AC_CHECK_HEADER(gssapi/gssapi.h, [
			AC_DEFINE(USE_GSSAPI,,
				  Define if you want to use GSSAPI)
			gss_libs="-lgssapi_krb5"
			AC_SUBST(gss_libs)
		])
	])
fi

# multithreading via pthreads
AC_ARG_ENABLE(pthreads,
        [AS_HELP_STRING([--enable-pthreads],[Enable multithreading via pthreads @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_pthreads="yes" ;;
          no) enable_pthreads="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-pthreads) ;;
         esac],
        [enable_pthreads=yes]
)

if test "x$enable_pthreads" != "xno"; then
  AC_CHECK_HEADERS(
    [pthread.h],
    [
      AC_CHECK_LIB(
        [pthread],
        [pthread_create],
        [
          AC_DEFINE([USE_PTHREADS], [1], [Multithreading support enabled.])
          pthreads_libs="-lpthread"
          pthreads_cflags="-pthread"
          AC_SUBST(pthreads_libs)
          AC_SUBST(pthreads_cflags)
        ],
        [AC_MSG_FAILURE([pthread is missing])],
        [-lpthread]
      )
    ],
    [AC_MSG_FAILURE([pthread is missing])]
  )
fi

# klogd
AC_ARG_ENABLE(klogd,
        [AS_HELP_STRING([--enable-klogd],[Integrated klogd functionality @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_klogd="yes" ;;
          no) enable_klogd="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-klogd) ;;
         esac],
        [enable_klogd="yes"]
)
if test "$enable_klogd" = "yes"; then
        AC_DEFINE(FEATURE_KLOGD, 1, [klogd functionality is integrated.])
fi

#
# SYSLOG_UNIXAF
#
AC_MSG_CHECKING(for SYSLOG_UNIXAF support)
AC_ARG_ENABLE([unix],
  [AS_HELP_STRING([--disable-unix], [Disable support for unix])],
  [
    if test "x${enableval}" = "xyes"; then
      AC_MSG_RESULT([yes])
      AC_DEFINE([SYSLOG_UNIXAF], [1], [Description])
    else
      AC_MSG_RESULT([no])
    fi
  ],
  [
    # enabled by default
    AC_MSG_RESULT([yes])
    AC_DEFINE([SYSLOG_UNIXAF], [1], [Description])
  ])

# inet
AC_ARG_ENABLE(inet,
        [AS_HELP_STRING([--enable-inet],[Enable networking support @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_inet="yes" ;;
          no) enable_inet="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-inet) ;;
         esac],
        [enable_inet="yes"]
)
if test "$enable_inet" = "yes"; then
        AC_DEFINE(SYSLOG_INET, 1, [network support is integrated.])
fi

#
# The following define determines whether the package adheres to the
# file system standard.
#
AC_MSG_CHECKING(for FSSTND support)
AC_ARG_ENABLE([fsstnd],
  [AS_HELP_STRING([--disable-fsstnd], [Disable support for FSSTND])],
  [
    if test "x${enableval}" = "xyes"; then
      AC_MSG_RESULT([yes])
      AC_DEFINE([FSSTND], [1], [Description])
    else
      AC_MSG_RESULT([no])
    fi
  ],
  [
    # enabled by default
    AC_MSG_RESULT([yes])
    AC_DEFINE([FSSTND], [1], [Description])
  ])

# debug
AC_ARG_ENABLE(debug,
        [AS_HELP_STRING([--enable-debug],[Enable debug mode @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_debug="yes" ;;
          no) enable_debug="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
         esac],
        [enable_debug="no"]
)
if test "$enable_debug" = "no"; then
        AC_DEFINE(NDEBUG, 1, [Defined if debug mode is disabled.])
fi



# MySQL support
AC_ARG_ENABLE(mysql,
        [AS_HELP_STRING([--enable-mysql],[Enable MySql database support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mysql="yes" ;;
          no) enable_mysql="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mysql) ;;
         esac],
        [enable_mysql=no]
)
if test "x$enable_mysql" = "xyes"; then
  AC_CHECK_HEADERS(
    [mysql/mysql.h],,
    [AC_MSG_FAILURE([MySQL is missing])]
  )
  AC_CHECK_PROG(
    [HAVE_MYSQL_CONFIG],
    [mysql_config],
    [yes],,,
  )
  if test "x${HAVE_MYSQL_CONFIG}" != "xyes"; then
    AC_MSG_FAILURE([mysql_config not found in PATH])
  fi
  AC_CHECK_LIB(
    [mysqlclient],
    [mysql_init],
    [mysql_cflags=`mysql_config --cflags`
     mysql_libs=`mysql_config --libs`
    ],
    [AC_MSG_FAILURE([MySQL library is missing])],
    [`mysql_config --libs`]
  )
fi
AM_CONDITIONAL(ENABLE_MYSQL, test x$enable_mysql = xyes)
AC_SUBST(mysql_cflags)
AC_SUBST(mysql_libs)

# PostgreSQL support
AC_ARG_ENABLE(pgsql,
        [AS_HELP_STRING([--enable-pgsql],[Enable PostgreSQL database support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_pgsql="yes" ;;
          no) enable_pgsql="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-pgsql) ;;
         esac],
        [enable_pgsql=no]
)
if test "x$enable_pgsql" = "xyes"; then
  AC_CHECK_PROG(
    [HAVE_PGSQL_CONFIG],
    [pg_config],
    [yes],,,
  )
  if test "x${HAVE_PGSQL_CONFIG}" != "xyes"; then
    AC_MSG_FAILURE([pg_config not found in PATH])
  fi
  AC_CHECK_LIB(
    [pq],
    [PQconnectdb],
    [pgsql_cflags="-I`pg_config --includedir`"
     pgsql_libs="`pg_config --libdir` -lpq"
    ],
    [AC_MSG_FAILURE([PgSQL library is missing])],
    [-L`pg_config --libdir`]
  )
fi
AM_CONDITIONAL(ENABLE_PGSQL, test x$enable_pgsql = xyes)
AC_SUBST(pgsql_cflags)
AC_SUBST(pgsql_libs)



AC_CONFIG_FILES([Makefile doc/Makefile plugins/ommysql/Makefile plugins/ompgsql/Makefile])
AC_OUTPUT

echo "****************************************************"
echo "rsyslog will be compiled with the following settings:"
echo
echo "Multithreading support enabled:           $enable_pthreads"
echo "Klogd functionality enabled:              $enable_klogd"
echo "Regular expressions support enabled:      $enable_regexp"
echo "Zlib compression support enabled:         $enable_zlib"
echo "MySql support enabled:                    $enable_mysql"
echo "PostgreSQL support enabled:               $enable_pgsql"
echo "Large file support enabled:               $enable_largefile"
echo "Networking support enabled:               $enable_inet"
echo "Enable GSSAPI Kerberos 5 support:         $want_gssapi_krb5"
echo "Debug mode enabled:                       $enable_debug"

