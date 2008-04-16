pkglib_LTLIBRARIES = ommysql.la

ommysql_la_SOURCES = ommysql.c ommysql.h
ommysql_la_CPPFLAGS =  $(rsrt_cflags) $(mysql_cflags) $(pthreads_cflags)
ommysql_la_LDFLAGS = -module -avoid-version
ommysql_la_LIBADD = $(mysql_libs)

EXTRA_DIST = createDB.sql contrib/delete_mysql 
