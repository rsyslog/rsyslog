pkglib_LTLIBRARIES = mmgrok.la

mmgrok_la_SOURCES = mmgrok.c /usr/lib/x86_64-linux-gnu/libgrok.so
mmgrok_la_CPPFLAGS = $(GLIB_CFLAGS) $(RSRT_CFLAGS) $(PTHREADS_CFLAGS)
mmgrok_la_LDFLAGS = -module -avoid-version
#mmgrok_la_LIBADD = $(GLIB_LIBS) -lgrok -ljson-c
mmgrok_la_LIBADD = $(GLIB_LIBS) -ljson-c

EXTRA_DIST = 
