pkglib_LTLIBRARIES = omkinesis.la
omkinesis_la_SOURCES = omkinesis.c
omkinesis_la_CPPFLAGS = $(RSRT_CFLAGS) $(CURL_CFLAGS)
omkinesis_la_LDFLAGS = -module -avoid-version
omkinesis_la_LIBADD = $(CURL_LIBS)
EXTRA_DIST = MODULE_METADATA.yaml
