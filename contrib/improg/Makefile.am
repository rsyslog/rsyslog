pkglib_LTLIBRARIES = improg.la

improg_la_SOURCES = improg.c
improg_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
improg_la_LDFLAGS = -module -avoid-version
improg_la_LIBADD = 

if ENABLE_LIBLOGGING_STDLOG
improg_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
improg_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif
