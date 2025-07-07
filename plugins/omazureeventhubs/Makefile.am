pkglib_LTLIBRARIES = omazureeventhubs.la

omazureeventhubs_la_SOURCES = omazureeventhubs.c
if ENABLE_QPIDPROTON_STATIC
omazureeventhubs_la_LDFLAGS = -module -avoid-version -Wl,-whole-archive -l:libqpid-proton-proactor-static.a -l:libqpid-proton-core-static.a -Wl,--no-whole-archive $(PTHREADS_LIBS) $(OPENSSL_LIBS) ${RT_LIBS} -lsasl2
omazureeventhubs_la_LIBADD =
else
omazureeventhubs_la_LDFLAGS = -module -avoid-version $(PROTON_PROACTOR_LIBS) $(PTHREADS_LIBS) $(OPENSSL_LIBS) -lm
omazureeventhubs_la_LIBADD =
endif
omazureeventhubs_la_CPPFLAGS = $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(CURL_CFLAGS) $(PROTON_PROACTOR_CFLAGS) -Wno-error=switch
EXTRA_DIST = 
