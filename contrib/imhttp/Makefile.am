pkglib_LTLIBRARIES = imhttp.la

imhttp_la_SOURCES = imhttp.c
imhttp_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(APU_CFLAGS)
imhttp_la_LDFLAGS = -module -avoid-version $(CIVETWEB_LIBS) $(APU_LIBS)
