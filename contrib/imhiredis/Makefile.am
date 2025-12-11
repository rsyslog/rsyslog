pkglib_LTLIBRARIES = imhiredis.la
imhiredis_la_SOURCES = imhiredis.c
imhiredis_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(HIREDIS_CFLAGS) $(HIREDIS_SSL_CFLAGS)
imhiredis_la_LDFLAGS = -module -avoid-version
imhiredis_la_LIBADD = $(HIREDIS_LIBS) $(HIREDIS_SSL_LIBS)

EXTRA_DIST =
