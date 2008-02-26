pkglib_LTLIBRARIES = imgssapi.la

imgssapi_la_SOURCES = imgssapi.c
imgssapi_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
imgssapi_la_LDFLAGS = $(mudflap_libs) -module -avoid-version
imgssapi_la_LIBADD = $(gss_libs) $(top_builddir)/libgssapi-misc.la
