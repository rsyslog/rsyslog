pkglib_LTLIBRARIES = imtcp.la

imtcp_la_SOURCES = imtcp.c
imtcp_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
imtcp_la_LDFLAGS = $(mudflap_libs) -module -avoid-version
imtcp_la_LIBADD = 
