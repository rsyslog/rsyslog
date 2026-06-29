pkglib_LTLIBRARIES = imkubernetes.la

imkubernetes_la_SOURCES = imkubernetes.c
imkubernetes_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(CURL_CFLAGS)
imkubernetes_la_LDFLAGS = -module -avoid-version
imkubernetes_la_LIBADD = $(CURL_LIBS)
