pkglib_LTLIBRARIES = omkinesis.la
omkinesis_la_SOURCES = omkinesis.c
omkinesis_la_CPPFLAGS = $(RSRT_CFLAGS) $(OMKINESIS_CURL_CFLAGS)
omkinesis_la_LDFLAGS = -module -avoid-version
omkinesis_la_LIBADD = $(OMKINESIS_CURL_LIBS)
EXTRA_DIST = MODULE_METADATA.yaml
