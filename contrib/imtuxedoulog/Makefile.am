pkglib_LTLIBRARIES = imtuxedoulog.la

imtuxedoulog_la_SOURCES = imtuxedoulog.c

imtuxedoulog_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
imtuxedoulog_la_LDFLAGS = -module -avoid-version
imtuxedoulog_la_LIBADD = 

if OS_AIX
imtuxedoulog_la_CPPFLAGS += -ma
endif


