pkglib_LTLIBRARIES = imdtls.la 
imdtls_la_DEPENDENCIES = ../../runtime/lmnsd_ossl.la
imdtls_la_SOURCES = imdtls.c
imdtls_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(OPENSSL_CFLAGS)
imdtls_la_LDFLAGS = -module -avoid-version
imdtls_la_LIBADD = $(OPENSSL_LIBS) ../../runtime/lmnsd_ossl.la
