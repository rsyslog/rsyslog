#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(rsyslog, 1.16.0, rsyslog@lists.adiscon.com.)
AM_INIT_AUTOMAKE(rsyslog, 1.16.0)
AC_CONFIG_SRCDIR([syslogd.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for libraries.

# Checks for header files.
AC_HEADER_RESOLV
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h locale.h netdb.h netinet/in.h paths.h stdio_ext.h stdlib.h string.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h syslog.h unistd.h utmp.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_FUNC_WAIT3
AC_CHECK_FUNCS([alarm gethostbyname gethostname gettimeofday memset regcomp select socket strcasecmp strchr strerror strndup strrchr strstr strtol strtoul uname])


# Ugly stuff 
AC_DEFINE(SYSV, 1, [I'd like to get rid of this.])
AC_DEFINE(SYSLOG_INET, 1, [Why there is this macro])


# Large file support
AC_ARG_ENABLE(largefile,
        [AC_HELP_STRING([--enable-largefile],[Enable large file support @<:@default=yes@:>@])],
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

# SQL support
AC_ARG_ENABLE(mysql,
        [AC_HELP_STRING([--enable-mysql],[Enable MySql database support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mysql="yes" ;;
          no) enable_mysql="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mysql) ;;
         esac],
        [enable_mysql=no]
)
if test "$enable_mysql" = "yes"; then
    AC_CHECK_PROG(MYSQL_CONFIG, mysql_config, yes)
    if test x$MYSQL_CONFIG = xyes; then
        AC_DEFINE(WITH_DB, 1, [MySql database support enabled])
        mysql_includes=`mysql_config --cflags`
        mysql_libs=`mysql_config --libs`
    else
        enable_mysql=no
    fi
fi
AM_CONDITIONAL(enable_mysql, test x$enable_mysql = xyes)
AC_SUBST(mysql_includes)
AC_SUBST(mysql_libs)

# Regular expressions
AC_ARG_ENABLE(regexp,
        [AC_HELP_STRING([--enable-regexp],[Enable regular expressions support @<:@default=yes@:>@])],
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
        [AC_HELP_STRING([--enable-zlib],[Enable zlib compression  support @<:@default=yes@:>@])],
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

# multithreading via pthreads
AC_ARG_ENABLE(pthreads,
        [AC_HELP_STRING([--enable-pthreads],[Enable multithreading via pthreads @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_pthreads="yes" ;;
          no) enable_pthreads="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-pthreads) ;;
         esac],
        [enable_pthreads=yes]
)
if test "$enable_pthreads" = "yes"; then
        AC_DEFINE(USE_PTHREADS, 1, [Multithreading support enabled.])
	pthreads_libs="-lpthread"
	AC_SUBST(pthreads_libs)
fi

# klogd
AC_ARG_ENABLE(klogd,
        [AC_HELP_STRING([--enable-klogd],[Integrated klogd functionality @<:@default=yes@:>@])],
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

# debug
AC_ARG_ENABLE(debug,
        [AC_HELP_STRING([--enable-debug],[Enable debug mode @<:@default=no@:>@])],
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

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo "****************************************************"
echo "rsyslog will be compiled with the followig settings:"
echo
echo "Multithreading support enabled:		$enable_pthreads"
echo "Klogd functionality enabled:		$enable_klogd"
echo "Regular expressions support enabled:	$enable_regexp"
echo "Zlib compression support enabled:	$enable_zlib"
echo "MySql support enabled:			$enable_mysql"
echo "Large file support enabled:		$enable_largefile"
echo "Debug mode enabled:			$enable_debug"

