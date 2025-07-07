pkglib_LTLIBRARIES = omsplunkhec.la

omsplunkhec_la_SOURCES = omsplunkhec.c
omsplunkhec_la_CPPFLAGS = $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(CURL_CFLAGS)
omsplunkhec_la_LDFLAGS = -module -avoid-version
omsplunkhec_la_LIBADD =  $(CURL_LIBS) $(LIBM)

EXTRA_DIST =
