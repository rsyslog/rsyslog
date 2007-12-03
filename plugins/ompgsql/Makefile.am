pkglibdir = $(libdir)/rsyslog
pkglib_LTLIBRARIES = ompgsql.la

ompgsql_la_SOURCES = ompgsql.c ompgsql.h
ompgsql_la_CPPFLAGS = $(pgsql_cflags) -I$(srcdir)/../..
ompgsql_la_LDFLAGS = -module -avoid-version
ompgsql_la_LIBADD = $(pgsql_libs)

EXTRA_DIST = createDB.sql
