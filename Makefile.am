sbin_PROGRAMS = rklogd rfc3195d rsyslogd

rklogd_SOURCES = \
	klogd.c \
	klogd.h \
	syslog.c \
	pidfile.c \
	pidfile.h \
	ksym.c \
	ksyms.h \
	ksym_mod.c \
	module.h

rfc3195d_SOURCES = rfc3195d.c rsyslog.h

rsyslogd_SOURCES = \
	syslogd.c \
	syslogd.h \
	pidfile.c \
	pidfile.h \
	template.c \
	outchannel.c \
	stringbuf.c \
	stringbuf.h \
	srUtils.c \
	srUtils.h \
	parse.c \
	parse.h \
	syslogd-types.h \
	template.h \
	outchannel.h \
	liblogging-stub.h \
	net.c \
	net.h \
	msg.c \
	msg.h \
	omshell.c \
	omshell.h \
	omusrmsg.c \
	omusrmsg.h \
	omfwd.c \
	omfwd.h \
	tcpsyslog.c \
	tcpsyslog.h \
	omfile.c \
	omfile.h \
	omdiscard.c \
	omdiscard.h \
	modules.c \
	modules.h \
	module-template.h \
	objomsr.c \
	objomsr.h \
	cfsysline.c \
	cfsysline.h \
	linkedlist.c \
	linkedlist.h \
	iminternal.c \
	iminternal.h \
	action.c \
	action.h

rsyslogd_CPPFLAGS = -D_PATH_MODDIR=\"$(pkglibdir)/\"
rsyslogd_LDADD = $(zlib_libs) $(pthreads_libs) -ldl
rsyslogd_LDFLAGS = -export-dynamic

man_MANS = rfc3195d.8 rklogd.8 rsyslogd.8 rsyslog.conf.5 

EXTRA_DIST = \
	redhat/rsyslog.conf \
	redhat/rsyslog.init \
	redhat/rsyslog.log \
	redhat/rsyslog.sysconfig \
	freebsd/rsyslogd \
	slackware/rc.rsyslogd \
	contrib/README \
	$(man_MANS)

SUBDIRS = doc

if ENABLE_MYSQL
SUBDIRS += plugins/ommysql
endif
