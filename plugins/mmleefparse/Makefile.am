pkglib_LTLIBRARIES = mmleefparse.la

if OS_AIX
LIBMATH = -lm
else
LIBMATH =
endif

mmleefparse_la_SOURCES = mmleefparse.c
mmleefparse_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS)
mmleefparse_la_LDFLAGS = -module -avoid-version $(LIBFASTJSON_LIBS) $(LIBMATH)
mmleefparse_la_LIBADD =

EXTRA_DIST =
