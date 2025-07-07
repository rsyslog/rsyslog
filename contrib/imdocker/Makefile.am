pkglib_LTLIBRARIES = imdocker.la

imdocker_la_SOURCES = imdocker.c
imdocker_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(CURL_CFLAGS) $(LIBLOGGING_STDLOG_CFLAGS)
imdocker_la_LDFLAGS = -module -avoid-version
imdocker_la_LIBADD = $(CURL_LIBS)
