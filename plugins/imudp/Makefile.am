pkglib_LTLIBRARIES = imudp.la

imudp_la_SOURCES = imudp.c
imudp_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
imudp_la_LDFLAGS = -module -avoid-version
imudp_la_LIBADD = $(IMUDP_LIBS)
