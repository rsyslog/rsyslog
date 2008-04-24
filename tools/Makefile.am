sbin_PROGRAMS =
man_MANS = 

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
	../action.h \
	../action.c \
	../threads.c \
	../threads.h \
	\
	../parse.c \
	../parse.h \
	\
	../outchannel.c \
	../outchannel.h \
	../template.c \
	../template.h \
	../conf.c \
	../conf.h

rsyslogd_CPPFLAGS =  $(pthreads_cflags) $(rsrt_cflags)
rsyslogd_LDADD = $(zlib_libs) $(pthreads_libs) $(dl_libs) $(rt_libs) $(rsrt_libs)
rsyslogd_LDFLAGS = -export-dynamic

man_MANS += rsyslogd.8 rsyslog.conf.5 
