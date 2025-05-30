pkglib_LTLIBRARIES = mmexternal.la

mmexternal_la_SOURCES = mmexternal.c
mmexternal_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS)
mmexternal_la_LDFLAGS = -module -avoid-version
mmexternal_la_LIBADD = 

if ENABLE_LIBLOGGING_STDLOG
mmexternal_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
mmexternal_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif

EXTRA_DIST = 
