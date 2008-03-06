pkglib_LTLIBRARIES = omsnmp.la

omsnmp_la_SOURCES = omsnmp.c omsnmp.h
omsnmp_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags)
omsnmp_la_LDFLAGS = -module -avoid-version
omsnmp_la_LIBADD =  $(snmp_libs)
