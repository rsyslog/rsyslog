pkglib_LTLIBRARIES = imtemplate.la

imtemplate_la_SOURCES = imtemplate.c
imtemplate_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags) $(mudflap_cflags)
imtemplate_la_LDFLAGS = $(mudflap_libs) -module -avoid-version
imtemplate_la_LIBADD = 
