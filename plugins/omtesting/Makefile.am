pkglib_LTLIBRARIES = omtesting.la

omtesting_la_SOURCES = omtesting.c
omtesting_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
if ENABLE_TESTBENCH
omtesting_la_CPPFLAGS += -DENABLE_TESTBENCH
endif
omtesting_la_LDFLAGS = -module -avoid-version
omtesting_la_LIBADD = 

if ENABLE_LIBLOGGING_STDLOG
omtesting_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
omtesting_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif
