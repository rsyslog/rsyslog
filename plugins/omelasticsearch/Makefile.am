pkglib_LTLIBRARIES = omelasticsearch.la

# TODO: replace cJSON
omelasticsearch_la_SOURCES = omelasticsearch.c cJSON/cjson.c  cJSON/cjson.h
omelasticsearch_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS)
omelasticsearch_la_LDFLAGS = -module -avoid-version
omelasticsearch_la_LIBADD =  $(CURL_LIBS) $(LIBM)

EXTRA_DIST = 
