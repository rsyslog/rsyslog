#!/bin/sh
# Run this to generate all the initial makefiles, etc.
# Licensed under ASL 2.0 (as of email conversation with original
# author Michael Biebl on 2016-12-04).

srcdir=$(dirname $0)
test -z "$srcdir" && srcdir=.

test -f $srcdir/configure.ac || {
    printf '**Error**: Directory "%s" does not look like the\n' "$srcdir"
    printf ' top-level package directory'
    exit 1
}

if test -z "$*"; then
  echo "**Warning**: I am going to run \`configure' with no arguments."
  echo "If you wish to pass any to it, please specify them on the"
  echo \`$0\'" command line."
  echo
fi

(cd $srcdir && autoreconf --verbose --force --install) || exit 1

conf_flags="--cache-file=config.cache"

if test x$NOCONFIGURE = x; then
  echo Running $srcdir/configure $conf_flags "$@" ...
  $srcdir/configure $conf_flags "$@" \
  && echo Now type \`make\' to compile. || exit 1
else
  echo Skipping configure process.
fi
