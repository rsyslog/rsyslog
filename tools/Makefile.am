sbin_PROGRAMS =
man_MANS = rsyslogd.8 rsyslog.conf.5 

sbin_PROGRAMS += rsyslogd
rsyslogd_SOURCES = \
	syslogd.c \
	syslogd.h \
	omshell.c \
	omshell.h \
	omusrmsg.c \
	omusrmsg.h \
	omfwd.c \
	omfwd.h \
	omfile.c \
	omfile.h \
	omdiscard.c \
	omdiscard.h \
	iminternal.c \
	iminternal.h \
	pidfile.c \
	pidfile.h \
	\
	../dirty.h

rsyslogd_CPPFLAGS =  $(pthreads_cflags) $(rsrt_cflags)
rsyslogd_LDADD = $(zlib_libs) $(pthreads_libs) $(rsrt_libs)
rsyslogd_LDFLAGS = -export-dynamic

EXTRA_DIST = $(man_MANS)
