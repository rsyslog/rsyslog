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
	ompipe.c \
	ompipe.h \
	omdiscard.c \
	omdiscard.h \
	pmrfc5424.c \
	pmrfc5424.h \
	pmrfc3164.c \
	pmrfc3164.h \
	smtradfile.c \
	smtradfile.h \
	smfile.c \
	smfile.h \
	smfwd.c \
	smfwd.h \
	smtradfwd.c \
	smtradfwd.h \
	iminternal.c \
	iminternal.h \
	pidfile.c \
	pidfile.h \
	\
	../dirty.h
rsyslogd_CPPFLAGS =  $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
rsyslogd_LDADD = $(ZLIB_LIBS) $(PTHREADS_LIBS) $(RSRT_LIBS) $(SOL_LIBS)
rsyslogd_LDFLAGS = -export-dynamic

if ENABLE_DIAGTOOLS
sbin_PROGRAMS += rsyslog_diag_hostname msggen zpipe
rsyslog_diag_hostname_SOURCES = gethostn.c
zpipe_SOURCES = zpipe.c
zpipe_LDADD = -lz
msggen_SOURCES = msggen.c
endif

EXTRA_DIST = $(man_MANS)
