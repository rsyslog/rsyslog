#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([librelp], [0.0.1], [rgerhards@hq.adiscon.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/relp.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
if test "$GCC" = "yes"
then CFLAGS="$CFLAGS -W -Wall -Wformat-security -Wshadow -Wcast-align -Wpointer-arith -Wmissing-format-attribute -g"
fi

AC_PROG_LIBTOOL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lnsl':
#AC_CHECK_LIB([nsl], [main])

# Checks for header files.
AC_HEADER_STDC
#AC_CHECK_HEADERS([])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
#AC_HEADER_TIME
#AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
#AC_CHECK_FUNCS([])

AC_CONFIG_FILES([Makefile \
		src/Makefile])
AC_OUTPUT
