pkglib_LTLIBRARIES = immark.la

immark_la_SOURCES = immark.c immark.h
immark_la_CPPFLAGS = $(RSRT_CFLAGS) -I$(top_srcdir) $(PTHREADS_CFLAGS)
immark_la_LDFLAGS = -module -avoid-version
immark_la_LIBADD = 

if ENABLE_LIBLOGGING_STDLOG
immark_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
immark_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif
