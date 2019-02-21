pkglib_LTLIBRARIES = imfile.la

if OS_AIX
LIBMATH = -lm
else
LIBMATH =
endif

imfile_la_SOURCES = imfile.c siphash.c
imfile_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(LIBLOGGING_STDLOG_CFLAGS)
imfile_la_LDFLAGS = -module -avoid-version $(LIBFASTJSON_LIBS) $(LIBMATH)
imfile_la_LIBADD = 
