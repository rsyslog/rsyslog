pkglib_LTLIBRARIES = omtesting.la

omtesting_la_SOURCES = omtesting.c
omtesting_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
omtesting_la_LDFLAGS = $(mudflap_libs) -module -avoid-version
omtesting_la_LIBADD = 
