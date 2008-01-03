pkglib_LTLIBRARIES = ompgsql.la

ompgsql_la_SOURCES = ompgsql.c ompgsql.h
ompgsql_la_CPPFLAGS = -I$(top_srcdir) $(pgsql_cflags)
ompgsql_la_LDFLAGS = -module -avoid-version
ompgsql_la_LIBADD = $(pgsql_libs)

EXTRA_DIST = createDB.sql
