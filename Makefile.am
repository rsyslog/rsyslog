sbin_PROGRAMS = rfc3195d rsyslogd

rfc3195d_SOURCES = rfc3195d.c rsyslog.h

rsyslogd_SOURCES = \
	datetime.c \
	datetime.h \
	errmsg.c \
	errmsg.h \
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

rsyslogd_CPPFLAGS =  -D_PATH_MODDIR=\"$(pkglibdir)/\" $(pthreads_cflags)
rsyslogd_LDADD = $(zlib_libs) $(pthreads_libs) $(dl_libs) $(rt_libs)
rsyslogd_LDFLAGS = -export-dynamic

# now come the library plugins
pkglib_LTLIBRARIES = tcpsrv.la

#
# TCP (Stream) Server support
#
tcpsrv_la_SOURCES = \
	tcps_sess.c \
	tcps_sess.h \
	tcpsrv.c \
	tcpsrv.h
tcpsrv_la_CPPFLAGS = $(pthreads_cflags)
tcpsrv_la_LDFLAGS = -module -avoid-version
tcpsrv_la_LIBADD = 

#
# network support
# 
if ENABLE_INET
pkglib_LTLIBRARIES += net.la
net_la_SOURCES = net.c net.h
net_la_CPPFLAGS = $(pthreads_cflags)
net_la_LDFLAGS = -module -avoid-version
net_la_LIBADD =
endif

#
# regular expression support
# 
if ENABLE_REGEXP
pkglib_LTLIBRARIES += lmregexp.la
lmregexp_la_SOURCES = regexp.c regexp.h
lmregexp_la_CPPFLAGS = $(pthreads_cflags)
lmregexp_la_LDFLAGS = -module -avoid-version
lmregexp_la_LIBADD =
endif

#
# gssapi support
# 
if ENABLE_GSSAPI
pkglib_LTLIBRARIES += gssutil.la
gssutil_la_SOURCES = gss-misc.c gss-misc.h
gssutil_la_CPPFLAGS = $(pthreads_cflags)
gssutil_la_LDFLAGS = -module -avoid-version
gssutil_la_LIBADD = $(gss_libs)
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
SUBDIRS += plugins/omgssapi plugins/imgssapi
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
