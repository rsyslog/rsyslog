pkglib_LTLIBRARIES = imhiredis.la
imhiredis_la_SOURCES = imhiredis.c
imhiredis_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(HIREDIS_CFLAGS)
imhiredis_la_LDFLAGS = -module -avoid-version
imhiredis_la_LIBADD = $(HIREDIS_LIBS)

EXTRA_DIST =
