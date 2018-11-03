pkglib_LTLIBRARIES = imklog.la
imklog_la_SOURCES = imklog.c imklog.h

# select klog "driver"
if ENABLE_IMKLOG_BSD
imklog_la_SOURCES += bsd.c
endif

if ENABLE_IMKLOG_LINUX
imklog_la_SOURCES += bsd.c
endif

imklog_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
imklog_la_LDFLAGS = -module -avoid-version
imklog_la_LIBADD = 

if ENABLE_LIBLOGGING_STDLOG
imklog_la_CPPFLAGS += $(LIBLOGGING_STDLOG_CFLAGS)
imklog_la_LDFLAGS += $(LIBLOGGING_STDLOG_LIBS)
endif
