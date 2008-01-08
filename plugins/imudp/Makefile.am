pkglib_LTLIBRARIES = imudp.la

imudp_la_SOURCES = imudp.c
imudp_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
imudp_la_LDFLAGS = $(mudflap_libs) -module -avoid-version
imudp_la_LIBADD = 
