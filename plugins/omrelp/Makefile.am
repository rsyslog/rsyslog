pkglib_LTLIBRARIES = omrelp.la

omrelp_la_SOURCES = omrelp.c
omrelp_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(RELP_CFLAGS)
omrelp_la_LDFLAGS = -module -avoid-version
omrelp_la_LIBADD = $(RELP_LIBS)
