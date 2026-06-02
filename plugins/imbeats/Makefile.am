pkglib_LTLIBRARIES = imbeats.la

imbeats_la_SOURCES = imbeats.c lj_parser.c lj_parser.h
imbeats_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(ZLIB_CFLAGS)
imbeats_la_LDFLAGS = -module -avoid-version $(ZLIB_LIBS)
imbeats_la_LIBADD =
