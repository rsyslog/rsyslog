pkglib_LTLIBRARIES = imtcp.la

imtcp_la_SOURCES = imtcp.c
imtcp_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
imtcp_la_LDFLAGS = -module -avoid-version
imtcp_la_LIBADD = 

if ENABLE_LIBLOGGING_STDLOG
imtcp_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
imtcp_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif
