pkglib_LTLIBRARIES = imrelp.la

imrelp_la_SOURCES = imrelp.c
imrelp_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(relp_cflags)
imrelp_la_LDFLAGS = -module -avoid-version
imrelp_la_LIBADD = $(relp_libs)
