pkglib_LTLIBRARIES = omhttpfs.la

omhttpfs_la_SOURCES = omhttpfs.c
omhttpfs_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(CURL_CFLAGS) $(JSON_C_CFLAGS)
omhttpfs_la_LDFLAGS = -module -avoid-version
omhttpfs_la_LIBADD = $(CURL_LIBS) $(JSON_C_LIBS)


EXTRA_DIST = 
