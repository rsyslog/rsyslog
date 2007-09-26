#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([rsyslog-ommysql],[1.19.8],[rsyslog@lists.adiscon.com.])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([ommysql.c])
AC_CONFIG_HEADER([ommysql-config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
if test "$GCC" = "yes"
then CFLAGS="$CFLAGS -W -Wall -Wformat-security -Wshadow -Wcast-align -Wpointer-arith -Wmissing-format-attribute -g"
fi
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_CANONICAL_HOST

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h syslog.h sys/param.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

# SQL support
AC_CHECK_HEADERS(
  [mysql/mysql.h],,
  [AC_MSG_FAILURE([MySQL headers are missing])]
)
AC_SEARCH_LIBS(
  [mysql_init],
  [mysqlclient],
  [mysql_cflags=`mysql_config --cflags`
   mysql_libs=`mysql_config --libs`
  ],
  [AC_MSG_FAILURE([MySQL library is missing])],
  [`mysql_config --libs`]
)
AC_SUBST(mysql_cflags)
AC_SUBST(mysql_libs)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
