pkglib_LTLIBRARIES = omdtls.la 
omdtls_la_DEPENDENCIES =
omdtls_la_SOURCES = omdtls.c
omdtls_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(OPENSSL_CFLAGS)
omdtls_la_LDFLAGS = -module -avoid-version
omdtls_la_LIBADD = $(OPENSSL_LIBS)
