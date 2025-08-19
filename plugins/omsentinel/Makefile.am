pkglib_LTLIBRARIES = omsentinel.la

omsentinel_la_SOURCES = omsentinel.c
omsentinel_la_CPPFLAGS = $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(CURL_CFLAGS)
omsentinel_la_LDFLAGS = -module -avoid-version
omsentinel_la_LIBADD =  $(CURL_LIBS) $(LIBM)

EXTRA_DIST = 
