pkglib_LTLIBRARIES = imfile.la

imfile_la_SOURCES = imfile.c
imfile_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
imfile_la_LDFLAGS = $(mudflap_libs) -module -avoid-version
imfile_la_LIBADD = 
