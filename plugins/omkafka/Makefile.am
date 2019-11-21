pkglib_LTLIBRARIES = omkafka.la

omkafka_la_SOURCES = omkafka.c
omkafka_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS)
if !ENABLE_KAFKA_STATIC
omkafka_la_LDFLAGS = -module -avoid-version $(LIBRDKAFKA_LIBS) -lm
endif
if ENABLE_KAFKA_STATIC
omkafka_la_LDFLAGS = -module -avoid-version -Wl,--whole-archive -l:librdkafka.a -Wl,--no-whole-archive -lssl -lpthread -lcrypto -lsasl2 -lz -llz4 -lrt -lm
endif
omkafka_la_LIBADD = 

EXTRA_DIST = 
