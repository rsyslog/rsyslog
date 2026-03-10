pkglib_LTLIBRARIES = omazuredce.la

omazuredce_la_SOURCES = omazuredce.c
omazuredce_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(CURL_CFLAGS) $(LIBFASTJSON_CFLAGS)
omazuredce_la_LDFLAGS = -module -avoid-version
omazuredce_la_LIBADD = $(CURL_LIBS) $(LIBFASTJSON_LIBS) $(PTHREADS_LIBS)

EXTRA_DIST = 
