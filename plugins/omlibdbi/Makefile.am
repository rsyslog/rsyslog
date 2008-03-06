pkglib_LTLIBRARIES = omlibdbi.la

omlibdbi_la_SOURCES = omlibdbi.c
omlibdbi_la_CPPFLAGS = -I$(top_srcdir) $(libdbi_cflags) $(pthreads_cflags)
omlibdbi_la_LDFLAGS = -module -avoid-version
omlibdbi_la_LIBADD = $(libdbi_libs)
