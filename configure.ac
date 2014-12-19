#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([rsyslog],[8.7.0.master],[rsyslog@lists.adiscon.com])
AM_INIT_AUTOMAKE([subdir-objects])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_SRCDIR([ChangeLog])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_USE_SYSTEM_EXTENSIONS

# check if valgrind is present
AC_CHECK_PROG(have_valgrind, [valgrind], [yes])
AM_CONDITIONAL(HAVE_VALGRIND, test x$have_valgrind = xyes)

# Checks for programs.
AC_PROG_LEX
AC_PROG_YACC
AC_PROG_CC
AC_PROG_CC_C99
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_CANONICAL_HOST

PKG_PROG_PKG_CONFIG

# modules we require
PKG_CHECK_MODULES(LIBESTR, libestr >= 0.1.9)
PKG_CHECK_MODULES([JSON_C], [json],, [
	PKG_CHECK_MODULES([JSON_C], [json-c],,)
])

save_CFLAGS="$CFLAGS"
save_LIBS="$LIBS"

CFLAGS="$CFLAGS $JSON_C_CFLAGS"
LIBS="$LIBS $JSON_C_LIBS"

# if int64 is supported, use it
AC_CHECK_LIB(json-c, json_object_new_object,,)
AC_CHECK_FUNCS(json_object_new_int64,,)

# look for newer API
AC_CHECK_FUNCS(json_tokener_error_desc,,)
AC_CHECK_FUNCS(json_object_object_get_ex,,)
AC_CHECK_TYPES([json_bool],,,[[#include <json_object.h>]])

CFLAGS="$save_CFLAGS"
LIBS="$save_LIBS"

case "${host}" in
  *-*-linux*)
    AC_DEFINE([OS_LINUX], [1], [Indicator for a Linux OS])
    os_type="linux"
  ;;
  *-*-*darwin*|*-*-dragonfly*|*-*-freebsd*|*-*-netbsd*|*-*-openbsd*)
    AC_DEFINE([OS_BSD], [1], [Indicator for a BSD OS])
    os_type="bsd"
  ;;
  *-*-kfreebsd*)
    # kernel is FreeBSD, but userspace is glibc - i.e. like linux
    # do not DEFINE OS_BSD
    os_type="bsd"
  ;;
  *-*-solaris*)
    os_type="solaris"
    AC_DEFINE([OS_SOLARIS], [1], [Indicator for a Solaris OS])
    AC_DEFINE([_POSIX_PTHREAD_SEMANTICS], [1], [Use POSIX pthread semantics])
    SOL_LIBS="-lsocket -lnsl"
    # Solaris libuuid does not ship with a pkgconfig file so override the appropriate
    # variables (but only if they have not been set by the user).
    LIBUUID_CFLAGS=${LIBUUID_CFLAGS:= }
    LIBUUID_LIBS=${LIBUUID_LIBS:=-luuid}
    AC_SUBST(SOL_LIBS)
  ;;
esac

AC_DEFINE_UNQUOTED([HOSTENV], "$host", [the host environment, can be queried via a system variable])

# Checks for libraries.
save_LIBS=$LIBS
LIBS=
AC_SEARCH_LIBS(clock_gettime, rt)
RT_LIBS=$LIBS
AC_SEARCH_LIBS(mq_getattr, rt)
RT_LIBS="$RT_LIBS $LIBS"
LIBS=
AC_SEARCH_LIBS(dlopen, dl)
DL_LIBS=$LIBS
LIBS=$save_LIBS

AC_SUBST(RT_LIBS)
AC_SUBST(DL_LIBS)

# Checks for header files.
AC_HEADER_RESOLV
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h libgen.h malloc.h fcntl.h locale.h netdb.h netinet/in.h paths.h stddef.h stdlib.h string.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h sys/stat.h sys/inotify.h unistd.h utmp.h utmpx.h sys/epoll.h sys/prctl.h])

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
AC_FUNC_STRERROR_R
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([flock inotify_init recvmmsg basename alarm clock_gettime gethostbyname gethostname gettimeofday localtime_r memset mkdir regcomp select setsid socket strcasecmp strchr strdup strerror strndup strnlen strrchr strstr strtol strtoul uname ttyname_r getline malloc_trim prctl epoll_create epoll_create1 fdatasync syscall lseek64])
AC_CHECK_TYPES([off64_t])

# getifaddrs is in libc (mostly) or in libsocket (eg Solaris 11) or not defined (eg Solaris 10)
AC_SEARCH_LIBS([getifaddrs], [socket], [AC_DEFINE(HAVE_GETIFADDRS, [1], [set define])])

# the check below is probably ugly. If someone knows how to do it in a better way, please
# let me know! -- rgerhards, 2010-10-06
AC_CHECK_DECL([SCM_CREDENTIALS], [AC_DEFINE(HAVE_SCM_CREDENTIALS, [1], [set define])], [], [#include <sys/types.h>
#include <sys/socket.h>])
AC_CHECK_DECL([SO_TIMESTAMP], [AC_DEFINE(HAVE_SO_TIMESTAMP, [1], [set define])], [], [#include <sys/types.h>
#include <sys/socket.h>])
AC_CHECK_DECL([SYS_gettid], [AC_DEFINE(HAVE_SYS_gettid, [1], [set define])], [], [#include <sys/syscall.h>])
AC_CHECK_MEMBER([struct sysinfo.uptime], [AC_DEFINE(HAVE_SYSINFO_UPTIME, [1], [set define])], [], [#include <sys/sysinfo.h>])
AC_CHECK_DECL([GLOB_NOMAGIC], [AC_DEFINE(HAVE_GLOB_NOMAGIC, [1], [set define])], [], [#include <glob.h>])

# Check for MAXHOSTNAMELEN
AC_MSG_CHECKING(for MAXHOSTNAMELEN)
AC_TRY_COMPILE([
	#include <sys/param.h>
	], [
	return MAXHOSTNAMELEN;
	]
	,
	AC_MSG_RESULT(yes)
	,
	# note: we use 1024 here, which should be far more than needed by any system. If that's too low, we simply
	# life with the need to change it. Most of the code doesn't need it anyways, but there are a few places 
	# where it actually is needed and it makes no sense to change them.
	AC_DEFINE(MAXHOSTNAMELEN, 1024, [Define with a value if your <sys/param.h> does not define MAXHOSTNAMELEN])
	AC_MSG_RESULT(no; defined as 64)
)

# Check for __builtin_expect()
AC_MSG_CHECKING([for __builtin_expect()])
AC_LINK_IFELSE([AC_LANG_PROGRAM(, return __builtin_expect(main != 0, 1))],
               [AC_DEFINE(HAVE_BUILTIN_EXPECT, 1,
                          Define to 1 if compiler supports __builtin_expect)
                AC_MSG_RESULT([yes])],
               [AC_MSG_RESULT([no])])

# check for availability of atomic operations
RS_ATOMIC_OPERATIONS
RS_ATOMIC_OPERATIONS_64BIT

# fall back to POSIX sems for atomic operations (cpu expensive)
AC_CHECK_HEADERS([semaphore.h sys/syscall.h])


# Additional module directories
AC_ARG_WITH(moddirs,
        [AS_HELP_STRING([--with-moddirs=DIRS],[Additional module search paths appended to @<:@$libdir/rsyslog@:>@])],
        [_save_IFS=$IFS ; IFS=$PATH_SEPARATOR ; moddirs=""
         for w in ${with_moddirs} ;
         do
                 case $w in
                 "") continue ;; */) ;; *)  w="${w}/" ;;
                 esac
                 for m in ${moddirs} ;
                 do
                         test "x$w" = "x${libdir}/${PACKAGE}/"   || \
                         test "x$w" = "x$m" || test "x$w" = "x/" && \
                         continue 2
                 done
                 case $moddirs in
                 "") moddirs="$w" ;; *) moddirs="${moddirs}:${w}" ;;
                 esac
         done ; IFS=$_save_IFS],[moddirs=""]
)
AM_CONDITIONAL(WITH_MODDIRS, test x$moddirs != x)
AC_SUBST(moddirs)


# Large file support
# http://www.gnu.org/software/autoconf/manual/html_node/System-Services.html#index-AC_005fSYS_005fLARGEFILE-1028
AC_SYS_LARGEFILE
case "${enable_largefile}" in
  no) ;;
  *) enable_largefile="yes" ;;
esac

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
AM_CONDITIONAL(ENABLE_REGEXP, test x$enable_regexp = xyes)
if test "$enable_regexp" = "yes"; then
        AC_DEFINE(FEATURE_REGEXP, 1, [Regular expressions support enabled.])
fi



AC_CHECK_HEADER(zlib.h, [zlib_header="yes"], [zlib_header="no" enable_zlib="false"])
if test "$zlib_header" = "yes"; then
	AC_CHECK_LIB(z, deflate,
		[AC_DEFINE(USE_NETZIP, 1, [Define if you want to enable zlib support]) 
		 ZLIB_LIBS="-lz"
		 AC_SUBST(ZLIB_LIBS)], enable_zlib="false")
fi


#gssapi
AC_ARG_ENABLE(gssapi_krb5,
	[AS_HELP_STRING([--enable-gssapi-krb5],[Enable GSSAPI Kerberos 5 support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_gssapi_krb5="yes" ;;
          no) enable_gssapi_krb5="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-gssapi-krb5) ;;
         esac],
	[enable_gssapi_krb5=no]
)
if test $enable_gssapi_krb5 = yes; then
	AC_CHECK_LIB(gssapi_krb5, gss_acquire_cred, [
		AC_CHECK_HEADER(gssapi/gssapi.h, [
			AC_DEFINE(USE_GSSAPI,,
				  Define if you want to use GSSAPI)
			GSS_LIBS="-lgssapi_krb5"
			AC_SUBST(GSS_LIBS)
		])
	])
fi
AM_CONDITIONAL(ENABLE_GSSAPI, test x$enable_gssapi_krb5 = xyes)


# multithreading via pthreads
AC_CHECK_HEADERS(
    [pthread.h],
    [
      AC_CHECK_LIB(
        [pthread],
        [pthread_create],
        [
          AC_DEFINE([USE_PTHREADS], [1], [Multithreading support enabled.])
          PTHREADS_LIBS="-lpthread"
 	  case "${os_type}" in
 	  solaris) PTHREADS_CFLAGS="-pthreads" ;;
 	  *)       PTHREADS_CFLAGS="-pthread" ;;
 	  esac
          AC_SUBST(PTHREADS_LIBS)
          AC_SUBST(PTHREADS_CFLAGS)
        ],
        [AC_MSG_FAILURE([pthread is missing])],
        [-lpthread]
      )
    ],
    [AC_MSG_FAILURE([pthread is missing])]
)

AC_CHECK_FUNCS(
    [pthread_setschedparam],
    [
      rsyslog_have_pthread_setschedparam=yes
    ],
    [
      rsyslog_have_pthread_setschedparam=no
    ]
)
AC_CHECK_HEADERS(
    [sched.h],
    [
      rsyslog_have_sched_h=yes
    ],
    [
      rsyslog_have_sched_h=no
    ]
)
if test "$rsyslog_have_pthread_setschedparam" = "yes" -a "$rsyslog_have_sched_h" = "yes"; then
	save_LIBS=$LIBS
	LIBS=
	AC_SEARCH_LIBS(sched_get_priority_max, rt)
	if test "x$ac_cv_search" != "xno"; then
		AC_CHECK_FUNCS(sched_get_priority_max)
	fi
	IMUDP_LIBS=$LIBS
	AC_SUBST(IMUDP_LIBS)
	LIBS=$save_LIBS
fi


# klog
AC_ARG_ENABLE(klog,
        [AS_HELP_STRING([--enable-klog],[Integrated klog functionality @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_klog="yes" ;;
          no) enable_klog="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-klog) ;;
         esac],
        [enable_klog="yes"]
)
AM_CONDITIONAL(ENABLE_IMKLOG, test x$enable_klog = xyes)
AM_CONDITIONAL(ENABLE_IMKLOG_BSD, test x$os_type = xbsd)
AM_CONDITIONAL(ENABLE_IMKLOG_LINUX, test x$os_type = xlinux)
AM_CONDITIONAL(ENABLE_IMKLOG_SOLARIS, test x$os_type = xsolaris)

# kmsg
AC_ARG_ENABLE(kmsg,
        [AS_HELP_STRING([--enable-kmsg],[Kmsg structured kernel logs functionality @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_kmsg="yes" ;;
          no) enable_kmsg="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-kmsg) ;;
         esac],
        [enable_kmsg="no"]
)
AM_CONDITIONAL(ENABLE_IMKMSG, test x$enable_kmsg = xyes)

# imjournal
AC_ARG_ENABLE(imjournal,
        [AS_HELP_STRING([--enable-imjournal],[Systemd journal message import @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_imjournal="yes" ;;
          no) enable_imjournal="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-imjournal) ;;
         esac],
        [enable_imjournal="no"]
)
if test "x$enable_imjournal" = "xyes"; then
	PKG_CHECK_MODULES([LIBSYSTEMD_JOURNAL], [libsystemd >= 209] ,, [
	    PKG_CHECK_MODULES([LIBSYSTEMD_JOURNAL], [libsystemd-journal >= 197])
	])
fi
AM_CONDITIONAL(ENABLE_IMJOURNAL, test x$enable_imjournal = xyes)

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
AM_CONDITIONAL(ENABLE_INET, test x$enable_inet = xyes)
if test "$enable_inet" = "yes"; then
        AC_DEFINE(SYSLOG_INET, 1, [network support is integrated.])
fi

# jemalloc
AC_ARG_ENABLE(jemalloc,
        [AS_HELP_STRING([--enable-jemalloc],[Enable jemalloc support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_jemalloc="yes" ;;
          no) enable_jemalloc="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-jemalloc) ;;
         esac],
        [enable_jemalloc="no"]
)
AM_CONDITIONAL(ENABLE_JEMALLOC, test x$enable_jemalloc = xyes)
if test "$enable_jemalloc" = "yes"; then
  AC_CHECK_LIB(
    [jemalloc],
    [malloc_stats_print],
    [RT_LIBS="$RT_LIBS -ljemalloc"
     AC_DEFINE(HAVE_JEMALLOC, 1, [jemalloc support is integrated.])
    ],
    [AC_MSG_FAILURE([jemalloc library is missing])],
    []
    )
fi


# support for unlimited select() syscall
AC_ARG_ENABLE(unlimited_select,
        [AS_HELP_STRING([--enable-unlimited-select],[Enable unlimited select() syscall @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_unlimited_select="yes" ;;
          no) enable_unlimited_select="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-unlimited-select) ;;
         esac],
        [enable_unlimited_select="no"]
)
if test "$enable_unlimited_select" = "yes"; then
        AC_DEFINE(USE_UNLIMITED_SELECT, 1, [If defined, the select() syscall won't be limited to a particular number of file descriptors.])
fi


# support for systemd unit files
AC_ARG_WITH([systemdsystemunitdir],
        AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
        [], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
if test "x$with_systemdsystemunitdir" != xno; then
        AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir" -a "x$with_systemdsystemunitdir" != xno ])


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
if test "$enable_debug" = "yes"; then
        AC_DEFINE(DEBUG, 1, [Defined if debug mode is enabled (its easier to check).])
fi
if test "$enable_debug" = "no"; then
        AC_DEFINE(NDEBUG, 1, [Defined if debug mode is disabled.])
fi


# runtime instrumentation
AC_ARG_ENABLE(rtinst,
        [AS_HELP_STRING([--enable-rtinst],[Enable runtime instrumentation mode @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_rtinst="yes" ;;
          no) enable_rtinst="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-rtinst) ;;
         esac],
        [enable_rtinst="no"]
)
if test "$enable_rtinst" = "yes"; then
        AC_DEFINE(RTINST, 1, [Defined if runtime instrumentation mode is enabled.])
fi


# total debugless: highest performance, but no way at all to enable debug 
# logging
AC_ARG_ENABLE(debugless,
        [AS_HELP_STRING([--enable-debugless],[Enable runtime instrumentation mode @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_debugless="yes" ;;
          no) enable_debugless="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-debugless) ;;
         esac],
        [enable_debugless="no"]
)
if test "$enable_debugless" = "yes"; then
        AC_DEFINE(DEBUGLESS, 1, [Defined if debugless mode is enabled.])
fi


# valgrind
AC_ARG_ENABLE(valgrind,
        [AS_HELP_STRING([--enable-valgrind],[Enable valgrind support settings @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_valgrind="yes" ;;
          no) enable_valgrind="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-valgrind) ;;
         esac],
        [enable_valgrind="no"]
)
if test "$enable_valgrind" = "yes"; then
        AC_DEFINE(VALGRIND, 1, [Defined if valgrind support settings are to be enabled (e.g. prevents dlclose()).])
fi


# memcheck
AC_ARG_ENABLE(memcheck,
        [AS_HELP_STRING([--enable-memcheck],[Enable extended memory check support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_memcheck="yes" ;;
          no) enable_memcheck="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-memcheck) ;;
         esac],
        [enable_memcheck="no"]
)
if test "$enable_memcheck" = "yes"; then
        AC_DEFINE(MEMCHECK, 1, [Defined if memcheck support settings are to be enabled (e.g. prevents dlclose()).])
fi


# compile diagnostic tools (small helpers usually not needed)
AC_ARG_ENABLE(diagtools,
        [AS_HELP_STRING([--enable-diagtools],[Enable diagnostic tools @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_diagtools="yes" ;;
          no) enable_diagtools="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-diagtools) ;;
         esac],
        [enable_diagtools=no]
)
AM_CONDITIONAL(ENABLE_DIAGTOOLS, test x$enable_diagtools = xyes)


# compile end-user tools
AC_ARG_ENABLE(usertools,
        [AS_HELP_STRING([--enable-usertools],[Enable end user tools @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_usertools="yes" ;;
          no) enable_usertools="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-usertools) ;;
         esac],
        [enable_usertools=no]
)
AM_CONDITIONAL(ENABLE_USERTOOLS, test x$enable_usertools = xyes)


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
  AC_CHECK_PROG(
    [HAVE_MYSQL_CONFIG],
    [mysql_config],
    [yes],,,
  )
  if test "x${HAVE_MYSQL_CONFIG}" != "xyes"; then
    AC_MSG_FAILURE([mysql_config not found in PATH - usually a package named mysql-dev, libmysql-dev or similar, is missing - install it to fix this issue])
  fi
  AC_CHECK_LIB(
    [mysqlclient],
    [mysql_init],
    [MYSQL_CFLAGS=`mysql_config --cflags`
     MYSQL_LIBS=`mysql_config --libs`
    ],
    [AC_MSG_FAILURE([MySQL library is missing])],
    [`mysql_config --libs`]
  )
  AC_MSG_CHECKING(if we have mysql_library_init)
  save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $MYSQL_CFLAGS"
  save_LIBS="$LIBS"
  LIBS="$LIBS $MYSQL_LIBS"
  AC_TRY_LINK(
    [#include <mysql.h>
     #include <stdio.h>],
    [mysql_library_init(0, NULL, NULL)],
    [have_mysql_library_init=yes],
    [have_mysql_library_init=no])
  CFLAGS="$save_CFLAGS"
  LIBS="$save_LIBS"
fi
AM_CONDITIONAL(ENABLE_MYSQL, test x$enable_mysql = xyes)
if test "$have_mysql_library_init" = "yes"; then
  AC_DEFINE([HAVE_MYSQL_LIBRARY_INIT], [1], [mysql_library_init available])
fi
AC_SUBST(MYSQL_CFLAGS)
AC_SUBST(MYSQL_LIBS)


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
    [PGSQL_CFLAGS="-I`pg_config --includedir`"
     PGSQL_LIBS="-L`pg_config --libdir` -lpq"
    ],
    [AC_MSG_FAILURE([PgSQL library is missing])],
    [-L`pg_config --libdir`]
  )
fi
AM_CONDITIONAL(ENABLE_PGSQL, test x$enable_pgsql = xyes)
AC_SUBST(PGSQL_CFLAGS)
AC_SUBST(PGSQL_LIBS)

# libdbi support
AC_ARG_ENABLE(libdbi,
        [AS_HELP_STRING([--enable-libdbi],[Enable libdbi database support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_libdbi="yes" ;;
          no) enable_libdbi="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-libdbi) ;;
         esac],
        [enable_libdbi=no]
)
if test "x$enable_libdbi" = "xyes"; then
  AC_CHECK_HEADERS(
    [dbi/dbi.h],,
    [AC_MSG_FAILURE([libdbi is missing])]
  )
  AC_CHECK_LIB(
    [dbi],
    [dbi_initialize],
    [LIBDBI_CFLAGS=""
     LIBDBI_LIBS="-ldbi"
    ],
    [AC_MSG_FAILURE([libdbi library is missing])]
  )
  AC_CHECK_LIB(
    [dbi],
    [dbi_initialize_r],
    [AC_DEFINE([HAVE_DBI_R], [1], [Define to 1 if libdbi supports the new plugin-safe interface])]
  )
  AC_CHECK_LIB(
    [dbi],
    [dbi_conn_transaction_begin],
    [AC_DEFINE([HAVE_DBI_TXSUPP], [1], [Define to 1 if libdbi supports transactions])]
  )
fi
AM_CONDITIONAL(ENABLE_OMLIBDBI, test x$enable_libdbi = xyes)
AC_SUBST(LIBDBI_CFLAGS)
AC_SUBST(LIBDBI_LIBS)

# SNMP support
AC_ARG_ENABLE(snmp,
        [AS_HELP_STRING([--enable-snmp],[Enable SNMP support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_snmp="yes" ;;
          no) enable_snmp="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-snmp) ;;
         esac],
        [enable_snmp=no]
)
if test "x$enable_snmp" = "xyes"; then
  AC_CHECK_HEADERS(
    [net-snmp/net-snmp-config.h],,
    [AC_MSG_FAILURE([Net-SNMP is missing])]
  )
  AC_CHECK_LIB(
    [netsnmp],
    [snmp_timeout],
    [SNMP_CFLAGS=""
     SNMP_LIBS="-lnetsnmp"
    ],
    [AC_MSG_FAILURE([Net-SNMP library is missing])]
  )
fi
AM_CONDITIONAL(ENABLE_SNMP, test x$enable_snmp = xyes)
AC_SUBST(SNMP_CFLAGS)
AC_SUBST(SNMP_LIBS)


# uuid support
AC_ARG_ENABLE(uuid,
        [AS_HELP_STRING([--enable-uuid],[Enable support for uuid generation @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_uuid="yes" ;;
          no) enable_uuid="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-uuid) ;;
         esac],
        [enable_uuid=yes]
)
if test "x$enable_uuid" = "xyes"; then
	PKG_CHECK_MODULES([LIBUUID], [uuid])
	AC_DEFINE(USE_LIBUUID, 1, [Define if you want to enable libuuid support])
fi
AM_CONDITIONAL(ENABLE_UUID, test x$enable_uuid = xyes)


# elasticsearch support
AC_ARG_ENABLE(elasticsearch,
        [AS_HELP_STRING([--enable-elasticsearch],[Enable elasticsearch output module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_elasticsearch="yes" ;;
          no) enable_elasticsearch="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-elasticsearch) ;;
         esac],
        [enable_elasticsearch=no]
)
if test "x$enable_elasticsearch" = "xyes"; then
	PKG_CHECK_MODULES([CURL], [libcurl])
	LT_LIB_M
fi
AM_CONDITIONAL(ENABLE_ELASTICSEARCH, test x$enable_elasticsearch = xyes)


# capability to enable elasticsearch testbench tests. This requries that an ES test
# environment is present on the local (127.0.0.1) machine. 
AC_ARG_ENABLE(elasticsearch_tests,
        [AS_HELP_STRING([--enable-elasticsearch-tests],[enable MySQL specific tests in testbench @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_elasticsearch_tests="yes" ;;
          no) enable_elasticsearch_tests="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-elasticsearch-tests) ;;
         esac],
        [enable_elasticsearch_tests=no]
)
AM_CONDITIONAL(ENABLE_ELASTICSEARCH_TESTS, test x$enable_elasticsearch_tests = xyes)


# GnuTLS support
AC_ARG_ENABLE(gnutls,
        [AS_HELP_STRING([--enable-gnutls],[Enable GNU TLS support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_gnutls="yes" ;;
          no) enable_gnutls="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-gnutls) ;;
         esac],
        [enable_gnutls=no]
)
if test "x$enable_gnutls" = "xyes"; then
	PKG_CHECK_MODULES(GNUTLS, gnutls >= 1.4.0)
	AC_DEFINE([ENABLE_GNUTLS], [1], [Indicator that GnuTLS is present])
fi
AM_CONDITIONAL(ENABLE_GNUTLS, test x$enable_gnutls = xyes)

# libgcrypt support
AC_ARG_ENABLE(libgcrypt,
        [AS_HELP_STRING([--enable-libgcrypt],[Enable log file encryption support (libgcrypt) @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_libgcrypt="yes" ;;
          no) enable_libgcrypt="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-libgcrypt) ;;
         esac],
        [enable_libgcrypt=yes]
)
if test "x$enable_libgcrypt" = "xyes"; then
	AC_PATH_PROG([LIBGCRYPT_CONFIG],[libgcrypt-config],[no])
        if test "x${LIBGCRYPT_CONFIG}" = "xno"; then
           AC_MSG_FAILURE([libgcrypt-config not found in PATH])
        fi
        AC_CHECK_LIB(
		[gcrypt],
        	[gcry_cipher_open],
        	[LIBGCRYPT_CFLAGS="`${LIBGCRYPT_CONFIG} --cflags`"
        	LIBGCRYPT_LIBS="`${LIBGCRYPT_CONFIG} --libs`"
        	],
        	[AC_MSG_FAILURE([libgcrypt is missing])],
        	[`${LIBGCRYPT_CONFIG} --libs --cflags`]
        	)
	AC_DEFINE([ENABLE_LIBGCRYPT], [1], [Indicator that LIBGCRYPT is present])
fi
AM_CONDITIONAL(ENABLE_LIBGCRYPT, test x$enable_libgcrypt = xyes)
AC_SUBST(LIBGCRYPT_CFLAGS)
AC_SUBST(LIBGCRYPT_LIBS)


# support for building the rsyslogd runtime
AC_ARG_ENABLE(rsyslogrt,
        [AS_HELP_STRING([--enable-rsyslogrt],[Build rsyslogrt @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_rsyslogrt="yes" ;;
          no) enable_rsyslogrt="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-rsyslogrt) ;;
         esac],
        [enable_rsyslogrt=yes]
)
if test "x$enable_rsyslogrt" = "xyes"; then
  RSRT_CFLAGS1="-I\$(top_srcdir)/runtime -I\$(top_srcdir) -I\$(top_srcdir)/grammar"
  RSRT_LIBS1="\$(top_builddir)/runtime/librsyslog.la"
fi
AM_CONDITIONAL(ENABLE_RSYSLOGRT, test x$enable_rsyslogrt = xyes)
RSRT_CFLAGS="\$(RSRT_CFLAGS1) \$(LIBESTR_CFLAGS) \$(JSON_C_CFLAGS)"
if test "$GCC" = "yes"
then RSRT_CFLAGS="$RSRT_CFLAGS -W -Wall -Wformat-security -Wshadow -Wcast-align -Wpointer-arith -Wmissing-format-attribute -Werror=implicit-function-declaration -g"
fi
RSRT_LIBS="\$(RSRT_LIBS1) \$(LIBESTR_LIBS) \$(JSON_C_LIBS)"
AC_SUBST(RSRT_CFLAGS1)
AC_SUBST(RSRT_LIBS1)
AC_SUBST(RSRT_CFLAGS)
AC_SUBST(RSRT_LIBS)


# support for NOT building rsyslogd (useful for source-based packaging systems)
AC_ARG_ENABLE(rsyslogd,
        [AS_HELP_STRING([--enable-rsyslogd],[Build rsyslogd @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_rsyslogd="yes" ;;
          no) enable_rsyslogd="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-rsyslogd) ;;
         esac],
        [enable_rsyslogd=yes]
)
AM_CONDITIONAL(ENABLE_RSYSLOGD, test x$enable_rsyslogd = xyes)


# capability to enable an extended testbench. By default, this is off. The reason
# for this switch is that some test simply take too long to execute them on a regular
# basis. So we enable to skip them, while the majority of tests can still be used. The
# idea is that at least "make distcheck" executes the extended testbench, and also
# developers should explicitely enable it after important changes. -- rgerhards, 2010-04-12
AC_ARG_ENABLE(extended_tests,
        [AS_HELP_STRING([--enable-extended-tests],[extended testbench @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_rsyslogd="yes" ;;
          no) enable_rsyslogd="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-extended-tests) ;;
         esac],
        [enable_extended_tests=no]
)
AM_CONDITIONAL(ENABLE_EXTENDED_TESTS, test x$enable_extended_tests = xyes)


# capability to enable MySQL testbench tests. This requries that a Syslog database
# with the default schema has been created on the local (127.0.0.1) MySQL server and
# a user "rsyslog" with password "testbench" exists, is able to login with default
# parameters and has sufficient (read: all) privileges on that database.
# rgerhards, 2011-03-09
AC_ARG_ENABLE(mysql_tests,
        [AS_HELP_STRING([--enable-mysql-tests],[enable MySQL specific tests in testbench @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mysql_tests="yes" ;;
          no) enable_mysql_tests="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mysql-tests) ;;
         esac],
        [enable_mysql_tests=no]
)
AM_CONDITIONAL(ENABLE_MYSQL_TESTS, test x$enable_mysql_tests = xyes)


# Mail support (so far we do not need a library, but we need to turn this on and off)
AC_ARG_ENABLE(mail,
        [AS_HELP_STRING([--enable-mail],[Enable mail support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mail="yes" ;;
          no) enable_mail="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mail) ;;
         esac],
        [enable_mail=no]
)
AM_CONDITIONAL(ENABLE_MAIL, test x$enable_mail = xyes)


# imdiag support
# This is a core testbench tool. You need to enable it if you want to
# use not only a small subset of the testbench.
AC_ARG_ENABLE(imdiag,
        [AS_HELP_STRING([--enable-imdiag],[Enable imdiag @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_imdiag="yes" ;;
          no) enable_imdiag="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-imdiag) ;;
         esac],
        [enable_imdiag=no]
)
if test "x$enable_imdiag" = "xyes"; then
	AC_DEFINE([ENABLE_IMDIAG], [1], [Indicator that IMDIAG is present])
fi
AM_CONDITIONAL(ENABLE_IMDIAG, test x$enable_imdiag = xyes)


# mmnormalize
AC_ARG_ENABLE(mmnormalize,
        [AS_HELP_STRING([--enable-mmnormalize],[Enable building mmnormalize support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mmnormalize="yes" ;;
          no) enable_mmnormalize="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmnormalize) ;;
         esac],
        [enable_mmnormalize=no]
)
if test "x$enable_mmnormalize" = "xyes"; then
	PKG_CHECK_MODULES(LIBLOGNORM, lognorm >= 1.0.2)

	save_CFLAGS="$CFLAGS"
	save_LIBS="$LIBS"

	CFLAGS="$CFLAGS $LIBLOGNORM_CFLAGS"
	LIBS="$LIBS $LIBLOGNORM_LIBS"

	AX_CHECK_DEFINED([[#include <lognorm-features.h>]],LOGNORM_REGEX_SUPPORTED,[lognorm_regex_supported="yes"],)

	CFLAGS="$save_CFLAGS"
	LIBS="$save_LIBS"
fi
AM_CONDITIONAL(LOGNORM_REGEX_SUPPORTED, test x$lognorm_regex_supported = xyes)
AM_CONDITIONAL(ENABLE_MMNORMALIZE, test x$enable_mmnormalize = xyes)

# mmnjsonparse
AC_ARG_ENABLE(mmjsonparse,
        [AS_HELP_STRING([--enable-mmjsonparse],[Enable building mmjsonparse support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mmjsonparse="yes" ;;
          no) enable_mmjsonparse="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmjsonparse) ;;
         esac],
        [enable_mmjsonparse=no]
)
AM_CONDITIONAL(ENABLE_MMJSONPARSE, test x$enable_mmjsonparse = xyes)

# mmaudit
AC_ARG_ENABLE(mmaudit,
        [AS_HELP_STRING([--enable-mmaudit],[Enable building mmaudit support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mmaudit="yes" ;;
          no) enable_mmaudit="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmaudit) ;;
         esac],
        [enable_mmaudit=no]
)
AM_CONDITIONAL(ENABLE_MMAUDIT, test x$enable_mmaudit = xyes)


# mmanon
AC_ARG_ENABLE(mmanon,
        [AS_HELP_STRING([--enable-mmanon],[Enable building mmanon support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mmanon="yes" ;;
          no) enable_mmanon="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmanon) ;;
         esac],
        [enable_mmanon=no]
)
AM_CONDITIONAL(ENABLE_MMANON, test x$enable_mmanon = xyes)


# mmutf8fix
AC_ARG_ENABLE(mmutf8fix,
        [AS_HELP_STRING([--enable-mmutf8fix],[Enable building mmutf8fix support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mmutf8fix="yes" ;;
          no) enable_mmutf8fix="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmutf8fix) ;;
         esac],
        [enable_mmutf8fix=no]
)
AM_CONDITIONAL(ENABLE_MMUTF8FIX, test x$enable_mmutf8fix = xyes)


# mmcount
AC_ARG_ENABLE(mmcount,
        [AS_HELP_STRING([--enable-mmcount],[Enable message counting @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mmcount="yes" ;;
          no) enable_mmcount="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmcount) ;;
         esac],
        [enable_mmcount=no]
)
AM_CONDITIONAL(ENABLE_MMCOUNT, test x$enable_mmcount = xyes)


# mmsequence
AC_ARG_ENABLE(mmsequence,
        [AS_HELP_STRING([--enable-mmsequence],[Enable sequence generator @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mmsequence="yes" ;;
          no) enable_mmsequence="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmsequence) ;;
         esac],
        [enable_mmsequence=no]
)
AM_CONDITIONAL(ENABLE_MMSEQUENCE, test x$enable_mmsequence = xyes)


# mmfields
AC_ARG_ENABLE(mmfields,
        [AS_HELP_STRING([--enable-mmfields],[Enable building mmfields support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mmfields="yes" ;;
          no) enable_mmfields="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmfields) ;;
         esac],
        [enable_mmfields=no]
)
AM_CONDITIONAL(ENABLE_MMFIELDS, test x$enable_mmfields = xyes)

# mmpstrucdata
AC_ARG_ENABLE(mmpstrucdata,
        [AS_HELP_STRING([--enable-mmpstrucdata],[Enable building mmpstrucdata support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mmpstrucdata="yes" ;;
          no) enable_mmpstrucdata="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmpstrucdata) ;;
         esac],
        [enable_mmpstrucdata=no]
)
AM_CONDITIONAL(ENABLE_MMPSTRUCDATA, test x$enable_mmpstrucdata = xyes)


# mmrfc5424addhmac
AC_ARG_ENABLE(mmrfc5424addhmac,
        [AS_HELP_STRING([--enable-mmrfc5424addhmac],[Enable building mmrfc5424addhmac support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mmrfc5424addhmac="yes" ;;
          no) enable_mmrfc5424addhmac="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmrfc5424addhmac) ;;
         esac],
        [enable_mmrfc5424addhmac=no]
)
if test "x$enable_mmrfc5424addhmac" = "xyes"; then
	PKG_CHECK_MODULES(OPENSSL, openssl >= 0.9.7)
#AC_CHECK_LIB([crypto],[CRYPTO_new_ex_data], [], [AC_MSG_ERROR([OpenSSL libraries required])])
#AC_CHECK_LIB([ssl],[SSL_library_init], [], [AC_MSG_ERROR([OpenSSL libraries required])])
#AC_CHECK_HEADERS([openssl/crypto.h openssl/x509.h openssl/pem.h openssl/ssl.h openssl/err.h],[],[AC_MSG_ERROR([OpenSSL headers required])])
fi
AM_CONDITIONAL(ENABLE_MMRFC5424ADDHMAC, test x$enable_mmrfc5424addhmac = xyes)


# RELP support
AC_ARG_ENABLE(relp,
        [AS_HELP_STRING([--enable-relp],[Enable RELP support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_relp="yes" ;;
          no) enable_relp="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-relp) ;;
         esac],
        [enable_relp=no]
)
if test "x$enable_relp" = "xyes"; then
	PKG_CHECK_MODULES(RELP, relp >= 1.2.5)
fi
AM_CONDITIONAL(ENABLE_RELP, test x$enable_relp = xyes)


# GuardTime support
AC_ARG_ENABLE(guardtime,
        [AS_HELP_STRING([--enable-guardtime],[Enable log file signing support (via GuardTime) @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_guardtime="yes" ;;
          no) enable_guardtime="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-guardtime) ;;
         esac],
        [enable_guardtime=no]
)
if test "x$enable_guardtime" = "xyes"; then
	PKG_CHECK_MODULES(GUARDTIME, libgt >= 0.3.1)
fi
AM_CONDITIONAL(ENABLE_GUARDTIME, test x$enable_guardtime = xyes)


# liblogging-stdlog support
AC_ARG_ENABLE(liblogging-stdlog,
        [AS_HELP_STRING([--enable-liblogging-stdlog],[Enable liblogging-stdlog support @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_liblogging_stdlog="yes" ;;
          no) enable_liblogging_stdlog="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-liblogging-stdlog) ;;
         esac],
        [enable_liblogging_stdlog=yes]
)
if test "x$enable_liblogging_stdlog" = "xyes"; then
        PKG_CHECK_MODULES(LIBLOGGING_STDLOG, liblogging-stdlog >= 1.0.3,
		AC_DEFINE(HAVE_LIBLOGGING_STDLOG, 1, [Define to 1 if liblogging-stdlog is available.])
        )
fi
AM_CONDITIONAL(ENABLE_LIBLOGGING_STDLOG, test x$enable_liblogging_stdlog = xyes)


# RFC 3195 support
AC_ARG_ENABLE(rfc3195,
        [AS_HELP_STRING([--enable-rfc3195],[Enable RFC3195 support @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_rfc3195="yes" ;;
          no) enable_rfc3195="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-rfc3195) ;;
         esac],
        [enable_rfc3195=no]
)
if test "x$enable_rfc3195" = "xyes"; then
	PKG_CHECK_MODULES(LIBLOGGING, liblogging-rfc3195 >= 1.0.1)
fi
AM_CONDITIONAL(ENABLE_RFC3195, test x$enable_rfc3195 = xyes)


# enable/disable the testbench (e.g. because some important parts
# are missing)
AC_ARG_ENABLE(testbench,
        [AS_HELP_STRING([--enable-testbench],[testbench enabled @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_testbench="yes" ;;
          no) enable_testbench="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-testbench) ;;
         esac],
        [enable_testbench=no]
)
AM_CONDITIONAL(ENABLE_TESTBENCH, test x$enable_testbench = xyes)
if test "x$enable_testbench" = "xyes"; then
	if test "x$enable_imdiag" != "xyes"; then
		AC_MSG_ERROR("--enable-testbench requires --enable-imdiag")
	fi
fi


# settings for the file input module
AC_ARG_ENABLE(imfile,
        [AS_HELP_STRING([--enable-imfile],[file input module enabled @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_imfile="yes" ;;
          no) enable_imfile="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-imfile) ;;
         esac],
        [enable_imfile=no]
)
AM_CONDITIONAL(ENABLE_IMFILE, test x$enable_imfile = xyes)


# settings for the door input module (under solaris, thus default off)
AC_ARG_ENABLE(imsolaris,
        [AS_HELP_STRING([--enable-imsolaris],[solaris input module enabled @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_imsolaris="yes" ;;
          no) enable_imsolaris="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-imsolaris) ;;
         esac],
        [enable_imsolaris=no]
)
AM_CONDITIONAL(ENABLE_IMSOLARIS, test x$enable_imsolaris = xyes)

# settings for the ptcp input module
AC_ARG_ENABLE(imptcp,
        [AS_HELP_STRING([--enable-imptcp],[plain tcp input module enabled @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_imptcp="yes" ;;
          no) enable_imptcp="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-imptcp) ;;
         esac],
        [enable_imptcp=no]
)
AM_CONDITIONAL(ENABLE_IMPTCP, test x$enable_imptcp = xyes)


# settings for the pstats input module
AC_ARG_ENABLE(impstats,
        [AS_HELP_STRING([--enable-impstats],[periodic statistics module enabled @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_impstats="yes" ;;
          no) enable_impstats="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-impstats) ;;
         esac],
        [enable_impstats=no]
)
AM_CONDITIONAL(ENABLE_IMPSTATS, test x$enable_impstats = xyes)


# settings for the omprog output module
AC_ARG_ENABLE(omprog,
        [AS_HELP_STRING([--enable-omprog],[Compiles omprog module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omprog="yes" ;;
          no) enable_omprog="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omprog) ;;
         esac],
        [enable_omprog=no]
)
AM_CONDITIONAL(ENABLE_OMPROG, test x$enable_omprog = xyes)


# settings for omudpspoof
AC_ARG_ENABLE(omudpspoof,
        [AS_HELP_STRING([--enable-omudpspoof],[Compiles omudpspoof module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omudpspoof="yes" ;;
          no) enable_omudpspoof="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omudpspoof) ;;
         esac],
        [enable_omudpspoof=no]
)

if test "x$enable_omudpspoof" = "xyes"; then
  AC_CHECK_HEADERS(
    [libnet.h],,
    [AC_MSG_FAILURE([libnet is missing])]
  )
  AC_CHECK_LIB(
    [net],
    [libnet_init],
    [UDPSPOOF_CFLAGS=""
     UDPSPOOF_LIBS="-lnet"
    ],
    [AC_MSG_FAILURE([libnet is missing])]
  )
fi
AM_CONDITIONAL(ENABLE_OMUDPSPOOF, test x$enable_omudpspoof = xyes)
AC_SUBST(UDPSPOOF_CFLAGS)
AC_SUBST(UDPSPOOF_LIBS)


# settings for omstdout
AC_ARG_ENABLE(omstdout,
        [AS_HELP_STRING([--enable-omstdout],[Compiles stdout module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omstdout="yes" ;;
          no) enable_omstdout="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omstdout) ;;
         esac],
        [enable_omstdout=no]
)
AM_CONDITIONAL(ENABLE_OMSTDOUT, test x$enable_omstdout = xyes)

# settings for omjournal
AC_ARG_ENABLE(omjournal,
        [AS_HELP_STRING([--enable-omjournal],[Compiles omjournal @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omjournal="yes" ;;
          no) enable_omjournal="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omjournal) ;;
         esac],
        [enable_omjournal=no]
)
if test "x$enable_omjournal" = "xyes"; then
	PKG_CHECK_MODULES([LIBSYSTEMD_JOURNAL], [libsystemd >= 209] ,, [
	    PKG_CHECK_MODULES([LIBSYSTEMD_JOURNAL], [libsystemd-journal >= 197])
	])
fi
AM_CONDITIONAL(ENABLE_OMJOURNAL, test x$enable_omjournal = xyes)


# settings for pmlastmsg
AC_ARG_ENABLE(pmlastmsg,
        [AS_HELP_STRING([--enable-pmlastmsg],[Compiles lastmsg parser module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_pmlastmsg="yes" ;;
          no) enable_pmlastmsg="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-pmlastmsg) ;;
         esac],
        [enable_pmlastmsg=no]
)
AM_CONDITIONAL(ENABLE_PMLASTMSG, test x$enable_pmlastmsg = xyes)


# settings for pmcisconames
AC_ARG_ENABLE(pmcisconames,
        [AS_HELP_STRING([--enable-pmcisconames],[Compiles cisconames parser module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_pmcisconames="yes" ;;
          no) enable_pmcisconames="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-pmcisconames) ;;
         esac],
        [enable_pmcisconames=no]
)
AM_CONDITIONAL(ENABLE_PMCISCONAMES, test x$enable_pmcisconames = xyes)


# settings for pmciscoios
AC_ARG_ENABLE(pmciscoios,
        [AS_HELP_STRING([--enable-pmciscoios],[Compiles ciscoios parser module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_pmciscoios="yes" ;;
          no) enable_pmciscoios="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-pmciscoios) ;;
         esac],
        [enable_pmciscoios=no]
)
AM_CONDITIONAL(ENABLE_PMCISCOIOS, test x$enable_pmciscoios = xyes)


# settings for pmaixforwardedfrom
AC_ARG_ENABLE(pmaixforwardedfrom,
        [AS_HELP_STRING([--enable-pmaixforwardedfrom],[Compiles aixforwardedfrom parser module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_pmaixforwardedfrom="yes" ;;
          no) enable_pmaixforwardedfrom="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-pmaixforwardedfrom) ;;
         esac],
        [enable_pmaixforwardedfrom=no]
)
AM_CONDITIONAL(ENABLE_PMAIXFORWARDEDFROM, test x$enable_pmaixforwardedfrom = xyes)


# settings for pmsnare
AC_ARG_ENABLE(pmsnare,
        [AS_HELP_STRING([--enable-pmsnare],[Compiles snare parser module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_pmsnare="yes" ;;
          no) enable_pmsnare="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-pmsnare) ;;
         esac],
        [enable_pmsnare=no]
)
AM_CONDITIONAL(ENABLE_PMSNARE, test x$enable_pmsnare = xyes)


# settings for omruleset
AC_ARG_ENABLE(omruleset,
        [AS_HELP_STRING([--enable-omruleset],[Compiles ruleset forwarding module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omruleset="yes" ;;
          no) enable_omruleset="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omruleset) ;;
         esac],
        [enable_omruleset=no]
)
AM_CONDITIONAL(ENABLE_OMRULESET, test x$enable_omruleset = xyes)


# settings for omuxsock
AC_ARG_ENABLE(omuxsock,
        [AS_HELP_STRING([--enable-omuxsock],[Compiles omuxsock module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omuxsock="yes" ;;
          no) enable_omuxsock="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omuxsock) ;;
         esac],
        [enable_omuxsock=no]
)
AM_CONDITIONAL(ENABLE_OMUXSOCK, test x$enable_omuxsock = xyes)


# settings for mmsnmptrapd message modification module
AC_ARG_ENABLE(mmsnmptrapd,
        [AS_HELP_STRING([--enable-mmsnmptrapd],[Compiles mmsnmptrapd module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_mmsnmptrapd="yes" ;;
          no) enable_mmsnmptrapd="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmsnmptrapd) ;;
         esac],
        [enable_mmsnmptrapd=no]
)
AM_CONDITIONAL(ENABLE_MMSNMPTRAPD, test x$enable_mmsnmptrapd = xyes)


# settings for the omhdfs;
AC_ARG_ENABLE(omhdfs,
        [AS_HELP_STRING([--enable-omhdfs],[Compiles omhdfs template module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omhdfs="yes" ;;
          no) enable_omhdfs="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omhdfs) ;;
         esac],
        [enable_omhdfs=no]
)
if test "x$enable_omhdfs"; then
	AC_CHECK_HEADERS([hdfs.h hadoop/hdfs.h])
fi
AM_CONDITIONAL(ENABLE_OMHDFS, test x$enable_omhdfs = xyes)


#MONGODB SUPPORT

AC_ARG_ENABLE(ommongodb,
        [AS_HELP_STRING([--enable-ommongodb],[Compiles ommongodb template module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_ommongodb="yes" ;;
          no) enable_ommongodb="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-ommongodb) ;;
         esac],
        [enable_ommongodb=no]
)
if test "x$enable_ommongodb" = "xyes"; then
	PKG_CHECK_MODULES(LIBMONGO_CLIENT, libmongo-client >= 0.1.4)
fi
AM_CONDITIONAL(ENABLE_OMMONGODB, test x$enable_ommongodb = xyes)
# end of mongodb code

# BEGIN ZMQ3 INPUT SUPPORT
AC_ARG_ENABLE(imzmq3,
        [AS_HELP_STRING([--enable-imzmq3],[Compiles imzmq3 output module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_imzmq3="yes" ;;
          no) enable_imzmq3="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-imzmq3) ;;
         esac],
        [enable_imzmq3=no]
)
if test "x$enable_imzmq3" = "xyes"; then
	PKG_CHECK_MODULES(CZMQ, libczmq >= 1.1.0)
fi
AM_CONDITIONAL(ENABLE_IMZMQ3, test x$enable_imzmq3 = xyes)

# END ZMQ3 INPUT SUPPORT

# BEGIN CZMQ INPUT SUPPORT
AC_ARG_ENABLE(imczmq,
        [AS_HELP_STRING([--enable-imczmq],[Compiles imczmq output module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_imczmq="yes" ;;
          no) enable_imczmq="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-imczmq) ;;
         esac],
        [enable_imczmq=no]
)
if test "x$enable_imczmq" = "xyes"; then
	PKG_CHECK_MODULES(CZMQ, libczmq >= 3.0.0)
fi
AM_CONDITIONAL(ENABLE_IMCZMQ, test x$enable_imczmq = xyes)

# END CZMQ INPUT

# BEGIN ZMQ3 OUTPUT SUPPORT
AC_ARG_ENABLE(omzmq3,
        [AS_HELP_STRING([--enable-omzmq3],[Compiles omzmq3 output module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omzmq3="yes" ;;
          no) enable_omzmq3="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omzmq3) ;;
         esac],
        [enable_omzmq3=no]
)
if test "x$enable_omzmq3" = "xyes"; then
	PKG_CHECK_MODULES(CZMQ, libczmq >= 1.1.0)
fi
AM_CONDITIONAL(ENABLE_OMZMQ3, test x$enable_omzmq3 = xyes)

# END ZMQ3 SUPPORT

# BEGIN CZMQ OUTPUT SUPPORT
AC_ARG_ENABLE(omczmq,
        [AS_HELP_STRING([--enable-omczmq],[Compiles omczmq output module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omczmq="yes" ;;
          no) enable_omczmq="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omzmq3) ;;
         esac],
        [enable_omczmq=no]
)
if test "x$enable_omczmq" = "xyes"; then
   PKG_CHECK_MODULES(CZMQ, libczmq >= 3.0.0)
fi
AM_CONDITIONAL(ENABLE_OMCZMQ, test x$enable_omczmq = xyes)

# END CZMQ SUPPORT


# BEGIN RABBITMQ OUTPUT SUPPORT

AC_ARG_ENABLE(omrabbitmq,
        [AS_HELP_STRING([--enable-omrabbitmq],[Compiles omrabbitmq output module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omrabbitmq="yes" ;;
          no) enable_omrabbitmq="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omrabbitmq) ;;
         esac],
        [enable_omrabbitmq=no]
)
if test "x$enable_omrabbitmq" = "xyes"; then
        PKG_CHECK_MODULES(RABBITMQ, librabbitmq >= 0.2.0)
        AC_SUBST(RABBITMQ_CFLAGS)
        AC_SUBST(RABBITMQ_LIBS)
fi
AM_CONDITIONAL(ENABLE_OMRABBITMQ, test x$enable_omrabbitmq = xyes)

# END RABBITMQ SUPPORT

# HIREDIS SUPPORT

AC_ARG_ENABLE(omhiredis,
        [AS_HELP_STRING([--enable-omhiredis],[Compiles omhiredis template module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omhiredis="yes" ;;
          no) enable_omhiredis="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omhiredis) ;;
         esac],
        [enable_omhiredis=no]
)
#
if test "x$enable_omhiredis" = "xyes"; then
	PKG_CHECK_MODULES(HIREDIS, hiredis >= 0.10.1)
fi
AM_CONDITIONAL(ENABLE_OMHIREDIS, test x$enable_omhiredis = xyes)

# END HIREDIS SUPPORT


AC_CHECKING([if required man pages already exist])
have_to_generate_man_pages="no"

# man pages for libgcrypt module
if test "x$enable_usertools" = "xyes" && test "x$enable_libgcrypt" = "xyes"; then
    AC_CHECK_FILES(["tools/rscryutil.1" "tools/rsgtutil.1"],
        [],
        [have_to_generate_man_pages="yes"]
    )
fi

# man pages for GuardTime module
if test "x$enable_usertools" = "xyes" && test "x$enable_guardtime" = "xyes"; then
    AC_CHECK_FILES(["tools/rscryutil.1" "tools/rsgtutil.1"],
        [],
        [have_to_generate_man_pages="yes"]
    )
fi

AC_ARG_ENABLE(generate-man-pages,
        [AS_HELP_STRING([--enable-generate-man-pages],[Generate man pages from source @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) have_to_generate_man_pages="yes" ;;
          no) have_to_generate_man_pages="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-generate-man-pages) ;;
         esac],
        [have_to_generate_man_pages=yes]
)

if test "x$have_to_generate_man_pages" = "xyes"; then
    AC_MSG_RESULT([Some man pages are missing or --enable-generate-man-pages given. We need rst2man to generate man pages from source...])
else
    AC_MSG_RESULT([All required man pages found. We don't need rst2man!])
fi

if test "x$have_to_generate_man_pages" = "xyes"; then
    # We need rst2man to generate our man pages
    AC_CHECK_PROGS([RST2MAN], [rst2man rst2man.py], [])
    if test -z "$RST2MAN"; then
        AC_MSG_ERROR([rst2man is required to build man pages. You can use the release tarball with pregenerated man pages to avoid this depedency. Use the --disable-generate-man-pages configure option in that case.])
    fi
fi

AC_CONFIG_FILES([Makefile \
		runtime/Makefile \
		compat/Makefile \
		grammar/Makefile \
		tools/Makefile \
		plugins/imudp/Makefile \
		plugins/imtcp/Makefile \
		plugins/im3195/Makefile \
		plugins/imgssapi/Makefile \
		plugins/imuxsock/Makefile \
		plugins/imjournal/Makefile \
		plugins/immark/Makefile \
		plugins/imklog/Makefile \
		plugins/omhdfs/Makefile \
		plugins/omprog/Makefile \
		plugins/mmexternal/Makefile \
		plugins/omstdout/Makefile \
		plugins/omjournal/Makefile \
		plugins/pmciscoios/Makefile \
		plugins/omruleset/Makefile \
		plugins/omuxsock/Makefile \
		plugins/imfile/Makefile \
		plugins/imsolaris/Makefile \
		plugins/imptcp/Makefile \
		plugins/impstats/Makefile \
		plugins/imrelp/Makefile \
		plugins/imdiag/Makefile \
		plugins/omtesting/Makefile \
		plugins/omgssapi/Makefile \
		plugins/ommysql/Makefile \
		plugins/ompgsql/Makefile \
		plugins/omrelp/Makefile \
		plugins/omlibdbi/Makefile \
		plugins/ommail/Makefile \
		plugins/omsnmp/Makefile \
		plugins/omudpspoof/Makefile \
		plugins/ommongodb/Makefile \
		plugins/mmnormalize/Makefile \
		plugins/mmjsonparse/Makefile \
		plugins/mmaudit/Makefile \
		plugins/mmanon/Makefile \
		plugins/mmutf8fix/Makefile \
		plugins/mmfields/Makefile \
		plugins/mmpstrucdata/Makefile \
		plugins/omelasticsearch/Makefile \
		plugins/mmsnmptrapd/Makefile \
		plugins/pmlastmsg/Makefile \
		contrib/pmsnare/Makefile \
		contrib/pmaixforwardedfrom/Makefile \
		contrib/omhiredis/Makefile \
		contrib/omrabbitmq/Makefile \
		contrib/imkmsg/Makefile \
		contrib/mmcount/Makefile \
		contrib/omzmq3/Makefile \
		contrib/omczmq/Makefile \
		contrib/imzmq3/Makefile \
		contrib/imczmq/Makefile \
		contrib/mmsequence/Makefile \
		contrib/mmrfc5424addhmac/Makefile \
		contrib/pmcisconames/Makefile \
		tests/Makefile])
AC_OUTPUT

echo "****************************************************"
echo "rsyslog will be compiled with the following settings:"
echo
echo "    Large file support enabled:               $enable_largefile"
echo "    Networking support enabled:               $enable_inet"
echo "    Regular expressions support enabled:      $enable_regexp"
echo "    Zlib compression support enabled:         $enable_zlib"
echo "    rsyslog runtime will be built:            $enable_rsyslogrt"
echo "    rsyslogd will be built:                   $enable_rsyslogd"
echo "    have to generate man pages:               $have_to_generate_man_pages"
echo "    Unlimited select() support enabled:       $enable_unlimited_select"
echo "    uuid support enabled:                     $enable_uuid"
echo "    Log file signing support:                 $enable_guardtime"
echo "    Log file encryption support:              $enable_libgcrypt"
echo "    anonymization support enabled:            $enable_mmanon"
echo "    message counting support enabled:         $enable_mmcount"
echo "    mmfields enabled:                         $enable_mmfields"
echo "    liblogging-stdlog support enabled:        $enable_liblogging_stdlog"
echo
echo "---{ input plugins }---"
echo "    Klog functionality enabled:               $enable_klog ($os_type)"
echo "    /dev/kmsg functionality enabled:          $enable_kmsg"
echo "    plain tcp input module enabled:           $enable_imptcp"
echo "    imdiag enabled:                           $enable_imdiag"
echo "    file input module enabled:                $enable_imfile"
echo "    Solaris input module enabled:             $enable_imsolaris"
echo "    periodic statistics module enabled:       $enable_impstats"
echo "    imzmq3 input module enabled:              $enable_imzmq3"
echo "    imczmq input module enabled:              $enable_imczmq"
echo "    imjournal input module enabled:           $enable_imjournal"
echo
echo "---{ output plugins }---"
echo "    Mail support enabled:                     $enable_mail"
echo "    omprog module will be compiled:           $enable_omprog"
echo "    omstdout module will be compiled:         $enable_omstdout"
echo "    omjournal module will be compiled:        $enable_omjournal"
echo "    omhdfs module will be compiled:           $enable_omhdfs"
echo "    omelasticsearch module will be compiled:  $enable_elasticsearch"
echo "    omruleset module will be compiled:        $enable_omruleset"
echo "    omudpspoof module will be compiled:       $enable_omudpspoof"
echo "    omuxsock module will be compiled:         $enable_omuxsock"
echo "    omzmq3 module will be compiled:           $enable_omzmq3"
echo "    omczmq module will be compiled:           $enable_omczmq"
echo "    omrabbitmq module will be compiled:       $enable_omrabbitmq"
echo
echo "---{ parser modules }---"
echo "    pmlastmsg module will be compiled:        $enable_pmlastmsg"
echo "    pmcisconames module will be compiled:     $enable_pmcisconames"
echo "    pmciscoios module will be compiled:       $enable_pmciscoios"
echo "    pmaixforwardedfrom module w.be compiled:  $enable_pmaixforwardedfrom"
echo "    pmsnare module will be compiled:          $enable_pmsnare"
echo
echo "---{ message modification modules }---"
echo "    mmnormalize module will be compiled:      $enable_mmnormalize"
echo "    mmjsonparse module will be compiled:      $enable_mmjsonparse"
echo "    mmjaduit module will be compiled:         $enable_mmaudit"
echo "    mmsnmptrapd module will be compiled:      $enable_mmsnmptrapd"
echo "    mmutf8fix enabled:                        $enable_mmutf8fix"
echo "    mmrfc5424addhmac enabled:                 $enable_mmrfc5424addhmac"
echo "    mmpstrucdata enabled:                     $enable_mmpstrucdata"
echo "    mmsequence enabled:                       $enable_mmsequence"
echo
echo "---{ database support }---"
echo "    MySql support enabled:                    $enable_mysql"
echo "    libdbi support enabled:                   $enable_libdbi"
echo "    PostgreSQL support enabled:               $enable_pgsql"
echo "    mongodb support enabled:                  $enable_ommongodb"
echo "    hiredis support enabled:                  $enable_omhiredis"
echo
echo "---{ protocol support }---"
echo "    GnuTLS network stream driver enabled:     $enable_gnutls"
echo "    GSSAPI Kerberos 5 support enabled:        $enable_gssapi_krb5"
echo "    RELP support enabled:                     $enable_relp"
echo "    SNMP support enabled:                     $enable_snmp"
echo
echo "---{ debugging support }---"
echo "    Testbench enabled:                        $enable_testbench"
echo "    Extended Testbench enabled:               $enable_extended_tests"
echo "    MySQL Tests enabled:                      $enable_mysql_tests"
echo "    Debug mode enabled:                       $enable_debug"
echo "    Runtime Instrumentation enabled:          $enable_rtinst"
echo "    (total) debugless mode enabled:           $enable_debugless"
echo "    Diagnostic tools enabled:                 $enable_diagtools"
echo "    End-User tools enabled:                   $enable_usertools"
echo "    Enhanced memory checking enabled:         $enable_memcheck"
echo "    Valgrind support settings enabled:        $enable_valgrind"
echo
