#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([librelp], [0.1.2], [rgerhards@adiscon.com])
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
save_LIBS=$LIBS
LIBS=
AC_SEARCH_LIBS(clock_gettime, rt)
rt_libs=$LIBS
LIBS=$save_LIBS

AC_SUBST(rt_libs)

# Checks for header files.
AC_HEADER_STDC
#AC_CHECK_HEADERS([])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
#AC_HEADER_TIME
#AC_STRUCT_TM

AC_CHECK_MEMBERS([struct sockaddr.sa_len],,,[$sa_includes])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
#AC_CHECK_FUNCS([])


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
echo "Debug mode enabled:          $enable_debug"

