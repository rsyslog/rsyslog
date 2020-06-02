pkglib_LTLIBRARIES = imkafka.la

if IMKAFKA_USE_DUMMY
imkafka_la_SOURCES = dummy.c
imkafka_la_LDFLAGS = -module -avoid-version
imkafka_la_CPPFLAGS = -I$(top_srcdir) $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) -D MODNAME=imkafka
else
imkafka_la_SOURCES = imkafka.c
imkafka_la_LDFLAGS = -module -avoid-version $(LIBRDKAFKA_LIBS)
imkafka_la_CPPFLAGS = -I$(top_srcdir) $(RSRT_CFLAGS) $(PTHREADS_CFLAGS)
endif
if ENABLE_KAFKA_STATIC
imkafka_la_LDFLAGS = -module -avoid-version -Wl,--whole-archive -l:librdkafka.a -Wl,--no-whole-archive -lssl -lpthread -lcrypto -lsasl2 -lz -llz4 -lrt # Static Linking now $(LIBRDKAFKA_LIBS)
endif
imkafka_la_LIBADD = 

EXTRA_DIST = 
