pkglib_LTLIBRARIES = omgssapi.la

omgssapi_la_SOURCES = omgssapi.c ../../module-template.h
omgssapi_la_CPPFLAGS = $(pgsql_cflags) -I$(srcdir)/../..
omgssapi_la_LDFLAGS = -module -avoid-version
omgssapi_la_LIBADD = $(gss_libs)
