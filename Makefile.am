sbin_PROGRAMS = rfc3195d rsyslogd

rfc3195d_SOURCES = rfc3195d.c rsyslog.h

rsyslogd_SOURCES = \
	syslogd.c \
	syslogd.h \
	sysvar.c \
	sysvar.h \
	vm.c \
	vm.h \
	vmstk.c \
	vmstk.h \
	vmprg.c \
	vmprg.h \
	vmop.c \
	vmop.h \
	debug.c \
	debug.h \
	glbl.h \
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
	threads.c \
	threads.h \
	stream.c \
	stream.h \
	var.c \
	var.h \
	wtp.c \
	wtp.h \
	wti.c \
	wti.h \
	queue.c \
	queue.h \
	sync.c \
	sync.h \
	net.c \
	net.h \
	obj.c \
	obj.h \
	obj-types.h \
	msg.c \
	msg.h \
	expr.c \
	expr.h \
	ctok.c \
	ctok.h \
	ctok_token.c \
	ctok_token.h \
	conf.c \
	conf.h \
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

rsyslogd_CPPFLAGS =  $(mudflap_cflags) -D_PATH_MODDIR=\"$(pkglibdir)/\" $(pthreads_cflags)
rsyslogd_LDADD = $(mudflap_libs) $(zlib_libs) $(pthreads_libs) $(dl_libs) $(rt_libs)
rsyslogd_LDFLAGS = -export-dynamic

if ENABLE_GSSAPI
noinst_LTLIBRARIES = libgssapi-misc.la
libgssapi_misc_la_SOURCES = gss-misc.c gss-misc.h
libgssapi_misc_la_LIBADD = $(gss_libs)
endif

man_MANS = rfc3195d.8 rsyslogd.8 rsyslog.conf.5 

EXTRA_DIST = \
	redhat/rsyslog.conf \
	redhat/rsyslog.init \
	redhat/rsyslog.log \
	redhat/rsyslog.sysconfig \
	freebsd/rsyslogd \
	slackware/rc.rsyslogd \
	contrib/README \
	rsyslog.conf \
	$(man_MANS)

SUBDIRS = . doc

SUBDIRS += plugins/immark plugins/imuxsock plugins/imtcp plugins/imudp plugins/omtesting

if ENABLE_IMKLOGD
SUBDIRS += plugins/imklog
endif

if ENABLE_GSSAPI
SUBDIRS += plugins/omgssapi
endif

if ENABLE_MYSQL
SUBDIRS += plugins/ommysql
endif

if ENABLE_OMLIBDBI
SUBDIRS += plugins/omlibdbi
endif

if ENABLE_PGSQL
SUBDIRS += plugins/ompgsql
endif

if ENABLE_SNMP
SUBDIRS += plugins/omsnmp
endif

if ENABLE_IMTEMPLATE
SUBDIRS += plugins/imtemplate
endif

if ENABLE_IMFILE
SUBDIRS += plugins/imfile
endif
