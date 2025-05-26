pkglib_LTLIBRARIES = mmjsonparse.la

if OS_AIX
LIBMATH = -lm
else
LIBMATH =
endif

mmjsonparse_la_SOURCES = mmjsonparse.c
mmjsonparse_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS)
mmjsonparse_la_LDFLAGS = -module -avoid-version $(LIBFASTJSON_LIBS) $(LIBMATH)
mmjsonparse_la_LIBADD = 

EXTRA_DIST = 
