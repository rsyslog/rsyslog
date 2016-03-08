pkglib_LTLIBRARIES = mmkubernetes.la

mmkubernetes_la_SOURCES = mmkubernetes.c
mmkubernetes_la_CPPFLAGS = $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(CURL_CFLAGS)
mmkubernetes_la_LDFLAGS = -module -avoid-version
mmkubernetes_la_LIBADD = $(CURL_LIBS)
