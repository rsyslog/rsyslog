pkglib_LTLIBRARIES = imuxsock.la

imuxsock_la_SOURCES = imuxsock.c ../../runtime/sd-daemon.c ../../runtime/sd-daemon.h
imuxsock_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
imuxsock_la_LDFLAGS = -module -avoid-version
imuxsock_la_LIBADD =
