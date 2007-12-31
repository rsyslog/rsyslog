pkglib_LTLIBRARIES = omtesting.la

omtesting_la_SOURCES = omtesting.c omtesting.h ../../module-template.h
omtesting_la_CPPFLAGS = $(mysql_cflags) -I$(srcdir)/../.. $(pthreads_cflags)
omtesting_la_LDFLAGS = -module -avoid-version
omtesting_la_LIBADD = $(mysql_libs)
