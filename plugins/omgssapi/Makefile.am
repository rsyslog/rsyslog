pkglib_LTLIBRARIES = omgssapi.la

omgssapi_la_SOURCES = omgssapi.c
omgssapi_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
omgssapi_la_LDFLAGS = $(mudflap_libs) -module -avoid-version
omgssapi_la_LIBADD = $(gss_libs) $(top_builddir)/libgssapi-misc.la
