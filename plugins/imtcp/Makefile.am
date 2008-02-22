pkglib_LTLIBRARIES = imtcp.la imgssapi.la

imtcp_la_SOURCES = imtcp.c
imtcp_la_CPPFLAGS = -DFORCE_NO_GSS=1 -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
imtcp_la_LDFLAGS = $(mudflap_libs) -module -avoid-version

imgssapi_la_SOURCES = imtcp.c
imgssapi_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
imgssapi_la_LDFLAGS = $(mudflap_libs) -module -avoid-version
imgssapi_la_LIBADD = $(gss_libs)
