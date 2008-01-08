pkglib_LTLIBRARIES = imuxsock.la

imuxsock_la_SOURCES = imuxsock.c
imuxsock_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
imuxsock_la_LDFLAGS = $(mudflap_libs) -module -avoid-version
imuxsock_la_LIBADD = 
