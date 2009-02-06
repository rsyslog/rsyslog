pkglib_LTLIBRARIES = ommysql.la

ommysql_la_SOURCES = ommysql.c ommysql.h
ommysql_la_CPPFLAGS =  $(RSRT_CFLAGS) $(MYSQL_CFLAGS) $(PTHREADS_CFLAGS)
ommysql_la_LDFLAGS = -module -avoid-version
ommysql_la_LIBADD = $(MYSQL_LIBS)

EXTRA_DIST = createDB.sql contrib/delete_mysql 
