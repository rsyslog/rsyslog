pkglib_LTLIBRARIES = mmgrok.la

mmgrok_la_SOURCES = mmgrok.c
mmgrok_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS)
mmgrok_la_LDFLAGS = -module -avoid-version
mmgrok_la_LIBADD = -lglib-2.0 -lgrok -ljson-c

EXTRA_DIST = 
