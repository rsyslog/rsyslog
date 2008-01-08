pkglib_LTLIBRARIES = immark.la

immark_la_SOURCES = immark.c immark.h
immark_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
immark_la_LDFLAGS = $(mudflap_libs) -module -avoid-version
immark_la_LIBADD = 
