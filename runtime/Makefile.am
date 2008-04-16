sbin_PROGRAMS =
man_MANS = 
noinst_LTLIBRARIES = librsyslog.la
pkglib_LTLIBRARIES =
#pkglib_LTLIBRARIES = librsyslog.la

librsyslog_la_SOURCES = \
	rsyslog.h \
	atomic.h \
	syslogd-types.h \
	module-template.h \
	obj-types.h \
	glbl.h \
	msg.c \
	msg.h \
	linkedlist.c \
	linkedlist.h \
	objomsr.c \
	objomsr.h \
	stringbuf.c \
	stringbuf.h \
	datetime.c \
	datetime.h \
	srutils.c \
	srUtils.h \
	errmsg.c \
	errmsg.h \
	debug.c \
	debug.h \
	obj.c \
	obj.h \
	modules.c \
	modules.h \
	sync.c \
	sync.h \
	expr.c \
	expr.h \
	ctok.c \
	ctok.h \
	ctok_token.c \
	ctok_token.h \
	stream.c \
	stream.h \
	var.c \
	var.h \
	wtp.c \
	wtp.h \
	wti.c \
	wti.h \
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
	queue.c \
	queue.h

librsyslog_la_CPPFLAGS = -D_PATH_MODDIR=\"$(pkglibdir)/\" -I$(top_srcdir) $(pthreads_cflags)
#librsyslog_la_LDFLAGS = -module -avoid-version
librsyslog_la_LIBADD = 

#
# regular expression support
# 
if ENABLE_REGEXP
pkglib_LTLIBRARIES += lmregexp.la
lmregexp_la_SOURCES = regexp.c regexp.h
lmregexp_la_CPPFLAGS = $(pthreads_cflags) $(rsrt_cflags)
lmregexp_la_LDFLAGS = -module -avoid-version $(rsrt_libs)
lmregexp_la_LIBADD =
endif

if ENABLE_INET
pkglib_LTLIBRARIES += lmnet.la
#
# network support
# 
lmnet_la_SOURCES = net.c net.h
lmnet_la_CPPFLAGS = $(pthreads_cflags) $(rsrt_cflags)
lmnet_la_LDFLAGS = -module -avoid-version $(rsrt_libs)
lmnet_la_LIBADD =

endif # if ENABLE_INET
