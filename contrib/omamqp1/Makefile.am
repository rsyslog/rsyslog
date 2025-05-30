pkglib_LTLIBRARIES = omamqp1.la

omamqp1_la_SOURCES = omamqp1.c
if ENABLE_QPIDPROTON_STATIC
omamqp1_la_LDFLAGS = -module -avoid-version $(PROTON_PROACTOR_LIBS) $(PTHREADS_LIBS) $(OPENSSL_LIBS) -lm
omamqp1_la_LDFLAGS = -module -avoid-version -Wl,-whole-archive -l:libqpid-proton-proactor-static.a -l:libqpid-proton-core-static.a -Wl,--no-whole-archive $(PTHREADS_LIBS) $(OPENSSL_LIBS) ${RT_LIBS} -lsasl2
omamqp1_la_LIBADD =
else
omamqp1_la_LDFLAGS = -module -avoid-version $(PROTON_PROACTOR_LIBS) $(PTHREADS_LIBS) $(OPENSSL_LIBS) -lm
omamqp1_la_LIBADD =
endif
omamqp1_la_CPPFLAGS = $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(CURL_CFLAGS) $(PROTON_PROACTOR_CFLAGS) -Wno-error=switch
EXTRA_DIST = 
