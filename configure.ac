#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([rsyslog],[4.6.3],[rsyslog@lists.adiscon.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([ChangeLog])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_GNU_SOURCE

# check for Java compiler
AC_CHECK_PROG(HAVE_JAVAC, [javac], [yes])
if test x"$HAVE_JAVAC" = x"yes"; then
        AC_MSG_WARN([no javac found, disabling features depending on it])
fi

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
if test "$GCC" = "yes"
then CFLAGS="$CFLAGS -W -Wall -Wformat-security -Wshadow -Wcast-align -Wpointer-arith -Wmissing-format-attribute -g"
fi
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_CANONICAL_HOST

PKG_PROG_PKG_CONFIG

case "${host}" in
  *-*-linux*)
    os_type="linux"
  ;;
  *-*-*darwin*|*-*-freebsd*|*-*-netbsd*|*-*-openbsd*)
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
    AC_SUBST(SOL_LIBS)
  ;;
esac

AC_DEFINE_UNQUOTED([HOSTENV], "$host", [the host environment, can be queried via a system variable])

# Checks for libraries.
save_LIBS=$LIBS
LIBS=
AC_SEARCH_LIBS(clock_gettime, rt)
RT_LIBS=$LIBS
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
AC_CHECK_HEADERS([arpa/inet.h libgen.h malloc.h fcntl.h locale.h netdb.h netinet/in.h paths.h stddef.h stdlib.h string.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h sys/stat.h syslog.h unistd.h utmp.h utmpx.h sys/epoll.h sys/prctl.h])

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
AC_CHECK_FUNCS([flock basename alarm clock_gettime gethostbyname gethostname gettimeofday localtime_r memset mkdir regcomp select setid socket strcasecmp strchr strdup strerror strndup strnlen strrchr strstr strtol strtoul uname ttyname_r epoll_wait getline malloc_trim prctl fdatasync])

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

# check for availability of atomic operations
RS_ATOMIC_OPERATIONS


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
AM_CONDITIONAL(ENABLE_ZLIB, test x$enable_zlib = xyes)
if test "$enable_zlib" = "yes"; then
	AC_CHECK_HEADER(zlib.h, [zlib_header="yes"], [zlib_header="no" enable_zlib="false"])
	if test "$zlib_header" = "yes"; then
		AC_CHECK_LIB(z, deflate,
                        [AC_DEFINE(USE_NETZIP, 1, [Define if you want to enable zlib support]) 
                         ZLIB_LIBS="-lz"
                         AC_SUBST(ZLIB_LIBS)], enable_zlib="false")
	fi
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
AC_ARG_ENABLE(pthreads,
        [AS_HELP_STRING([--enable-pthreads],[Enable multithreading via pthreads @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_pthreads="yes" ;;
          no) enable_pthreads="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-pthreads) ;;
         esac],
        [enable_pthreads=yes]
)

if test "x$enable_pthreads" = "xno"; then
	AC_MSG_ERROR(rsyslog v3 does no longer support single threading mode -- use a previous version for that);
fi

if test "x$enable_pthreads" != "xno"; then
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
AM_CONDITIONAL(ENABLE_INET, test x$enable_inet = xyes)
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
fi
AM_CONDITIONAL(ENABLE_MYSQL, test x$enable_mysql = xyes)
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


# oracle (OCI) support
AC_ARG_ENABLE(oracle,
	[AS_HELP_STRING([--enable-oracle],[Enable native Oracle database support @<:@default=no@:>@])],
	[case "${enableval}" in
	 yes) enable_oracle="yes" ;;
	 no) enable_oracle="no" ;;
	 *) AC_MSG_ERROR(bad value ${enableval} for --enable-oracle) ;;
	 esac],
	[enable_oracle=no]
)
if test "x$enable_oracle" = "xyes"; then
    AC_CHECK_PROG(
	[HAVE_ORACLE_CONFIG],
	[oracle-instantclient-config],
	[yes],,,
	)
    if test "x${HAVE_ORACLE_CONFIG}" != "xyes"; then
	AC_MSG_FAILURE([oracle-instantclient-config not found in PATH])
    fi
    AC_CHECK_LIB(
	[occi],
	[OCIEnvCreate],
	[ORACLE_CFLAGS="`oracle-instantclient-config --cflags`"
	ORACLE_LIBS="`oracle-instantclient-config --libs`"
	],
	[AC_MSG_FAILURE([Oracle (OCI) libraray is missing])],
	[`oracle-instantclient-config --libs --cflags`]
	)
fi
AM_CONDITIONAL(ENABLE_ORACLE, test x$enable_oracle = xyes)
AC_SUBST(ORACLE_CFLAGS)
AC_SUBST(ORACLE_LIBS)


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
fi
AM_CONDITIONAL(ENABLE_GNUTLS, test x$enable_gnutls = xyes)
AC_SUBST(GNUTLS_CFLAGS)
AC_SUBST(GNUTLS_LIBS)


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
  RSRT_CFLAGS="-I\$(top_srcdir)/runtime -I\$(top_srcdir)"
  RSRT_LIBS="\$(top_builddir)/runtime/librsyslog.la"
fi
AM_CONDITIONAL(ENABLE_RSYSLOGRT, test x$enable_rsyslogrt = xyes)
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


# imdiag support (so far we do not need a library, but we need to turn this on and off)
# note that we enable this be default, because an important point is to make
# it available to users who do not know much about how to handle things. It
# would complicate things if we first needed to tell them how to enable imdiag.
# rgerhards, 2008-07-25
AC_ARG_ENABLE(imdiag,
        [AS_HELP_STRING([--enable-imdiag],[Enable imdiag @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_imdiag="yes" ;;
          no) enable_imdiag="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-imdiag) ;;
         esac],
        [enable_imdiag=no]
)
AM_CONDITIONAL(ENABLE_IMDIAG, test x$enable_imdiag = xyes)


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
	PKG_CHECK_MODULES(RELP, relp >= 0.1.1)
fi
AM_CONDITIONAL(ENABLE_RELP, test x$enable_relp = xyes)
AC_SUBST(RELP_CFLAGS)
AC_SUBST(RELP_LIBS)


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
	PKG_CHECK_MODULES(LIBLOGGING, liblogging >= 0.7.1)
fi
AM_CONDITIONAL(ENABLE_RFC3195, test x$enable_rfc3195 = xyes)
AC_SUBST(LIBLOGGING_CFLAGS)
AC_SUBST(LIBLOGGING_LIBS)


# enable/disable the testbench (e.g. because some important parts
# are missing)
AC_ARG_ENABLE(testbench,
        [AS_HELP_STRING([--enable-testbench],[file input module enabled @<:@default=yes@:>@])],
        [case "${enableval}" in
         yes) enable_testbench="yes" ;;
          no) enable_testbench="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-testbench) ;;
         esac],
        [enable_testbench=yes]
)
if test "$enable_testbench" = "yes"; then
	if test x$HAVE_JAVAC = x; then
	   enable_testbench='no'
	fi
fi
AM_CONDITIONAL(ENABLE_TESTBENCH, test x$enable_testbench = xyes)


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

# This provides a vehicle to integrate custom modules, that are not
# part of rsyslog, into the build process. It is named cust1, so that
# additional such modules can easily be added.
AC_ARG_ENABLE(cust1,
        [AS_HELP_STRING([--enable-cust1],[Compiles stdout module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_cust1="yes" ;;
          no) enable_cust1="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-cust1) ;;
         esac],
        [enable_cust1=no]
)
AM_CONDITIONAL(ENABLE_CUST1, test x$enable_cust1 = xyes)


# settings for the template input module; copy and modify this code
# if you intend to add your own module. Be sure to replace imtemplate
# by the actual name of your module.
AC_ARG_ENABLE(imtemplate,
        [AS_HELP_STRING([--enable-imtemplate],[Compiles imtemplate template module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_imtemplate="yes" ;;
          no) enable_imtemplate="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-imtemplate) ;;
         esac],
        [enable_imtemplate=no]
)
#
# you may want to do some library checks here - see snmp, mysql, pgsql modules
# for samples
#
AM_CONDITIONAL(ENABLE_IMTEMPLATE, test x$enable_imtemplate = xyes)
# end of copy template - be sure to search for imtemplate to find everything!


# settings for the template output module; copy and modify this code
# if you intend to add your own module. Be sure to replace omtemplate
# by the actual name of your module.
AC_ARG_ENABLE(omtemplate,
        [AS_HELP_STRING([--enable-omtemplate],[Compiles omtemplate template module @<:@default=no@:>@])],
        [case "${enableval}" in
         yes) enable_omtemplate="yes" ;;
          no) enable_omtemplate="no" ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-omtemplate) ;;
         esac],
        [enable_omtemplate=no]
)
#
# you may want to do some library checks here - see snmp, mysql, pgsql modules
# for samples
#
AM_CONDITIONAL(ENABLE_OMTEMPLATE, test x$enable_omtemplate = xyes)
# end of copy template - be sure to search for omtemplate to find everything!


SHAVE_INIT


AC_CONFIG_FILES([Makefile \
		shave \
		shave-libtool \
		runtime/Makefile \
		tools/Makefile \
		doc/Makefile \
		plugins/imudp/Makefile \
		plugins/imtcp/Makefile \
		plugins/im3195/Makefile \
		plugins/imgssapi/Makefile \
		plugins/imuxsock/Makefile \
		plugins/immark/Makefile \
		plugins/imklog/Makefile \
		plugins/imtemplate/Makefile \
		plugins/omtemplate/Makefile \
		plugins/omprog/Makefile \
		plugins/omstdout/Makefile \
		plugins/imfile/Makefile \
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
		plugins/omoracle/Makefile \
		plugins/cust1/Makefile \
		tests/Makefile])
AC_OUTPUT

echo "****************************************************"
echo "rsyslog will be compiled with the following settings:"
echo
echo "    Multithreading support enabled:           $enable_pthreads"
echo "    Large file support enabled:               $enable_largefile"
echo "    Networking support enabled:               $enable_inet"
echo "    Regular expressions support enabled:      $enable_regexp"
echo "    Zlib compression support enabled:         $enable_zlib"
echo "    rsyslog runtime will be built:            $enable_rsyslogrt"
echo "    rsyslogd will be built:                   $enable_rsyslogd"
echo "    custom module 1 will be built:            $enable_cust1"
echo
echo "---{ input plugins }---"
echo "    Klog functionality enabled:               $enable_klog ($os_type)"
echo "    imdiag enabled:                           $enable_imdiag"
echo "    file input module enabled:                $enable_imfile"
echo "    input template module will be compiled:   $enable_imtemplate"
echo
echo "---{ output plugins }---"
echo "    Mail support enabled:                     $enable_mail"
echo "    omprog module will be compiled:           $enable_omprog"
echo "    omstdout module will be compiled:         $enable_omstdout"
echo "    output template module will be compiled:  $enable_omtemplate"
echo
echo "---{ database support }---"
echo "    MySql support enabled:                    $enable_mysql"
echo "    libdbi support enabled:                   $enable_libdbi"
echo "    PostgreSQL support enabled:               $enable_pgsql"
echo "    Oracle (OCI) support enabled:             $enable_oracle"
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
echo "    Debug mode enabled:                       $enable_debug"
echo "    Runtime Instrumentation enabled:          $enable_rtinst"
echo "    Diagnostic tools enabled:                 $enable_diagtools"
echo "    Valgrind support settings enabled:        $enable_valgrind"
echo
