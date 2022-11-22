pkglib_LTLIBRARIES = omkafka.la

if OMKAFKA_USE_DUMMY
omkafka_la_SOURCES = dummy.c
omkafka_la_LDFLAGS = -module -avoid-version
omkafka_la_CPPFLAGS = -I$(top_srcdir) $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) -D MODNAME=omkafka
else
omkafka_la_SOURCES = omkafka.c
if ENABLE_KAFKA_STATIC
omkafka_la_LDFLAGS = -module -avoid-version -Wl,--whole-archive -l:librdkafka-static.a -Wl,--no-whole-archive -ldl -lresolv -lcurl -lssl -lpthread -lcrypto -lsasl2 -lz -llz4 -lrt -lm
else
omkafka_la_LDFLAGS = -module -avoid-version $(LIBRDKAFKA_LIBS) -lm
endif
omkafka_la_CPPFLAGS = -I$(top_srcdir) $(RSRT_CFLAGS) $(PTHREADS_CFLAGS)
endif

omkafka_la_LIBADD = 
EXTRA_DIST = 
