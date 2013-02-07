pkglib_LTLIBRARIES = imjournal.la
imjournal_la_SOURCES = imjournal.c imjournal.h

CFLAGS += -lsystemd-journal

imjournal_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
imjournal_la_LDFLAGS = -module -avoid-version
imjournal_la_LIBADD = 
