pkglib_LTLIBRARIES = omsnmp.la

omsnmp_la_SOURCES = omsnmp.c
omsnmp_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
omsnmp_la_LDFLAGS = $(mudflap_libs) -module -avoid-version
omsnmp_la_LIBADD =  $(snmp_libs)
