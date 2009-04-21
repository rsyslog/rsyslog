sbin_PROGRAMS =
man_MANS = 
noinst_LTLIBRARIES = librsyslog.la
pkglib_LTLIBRARIES =
#pkglib_LTLIBRARIES = librsyslog.la

librsyslog_la_SOURCES = \
	rsyslog.c \
	rsyslog.h \
	atomic.h \
	syslogd-types.h \
	module-template.h \
	obj-types.h \
	nsd.h \
	glbl.h \
	glbl.c \
	conf.c \
	conf.h \
	parser.h \
	parser.c \
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
	queue.h \
	cfsysline.c \
	cfsysline.h \
	\
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
	../template.h
# the files with ../ we need to work on - so that they either become part of the
# runtime or will no longer be needed. -- rgerhards, 2008-06-13

if WITH_MODDIRS
librsyslog_la_CPPFLAGS = -D_PATH_MODDIR=\"$(pkglibdir)/:$(moddirs)\" $(PTHREADS_CFLAGS)
else
librsyslog_la_CPPFLAGS = -D_PATH_MODDIR=\"$(pkglibdir)/\" -I$(top_srcdir) $(PTHREADS_CFLAGS)
endif
#librsyslog_la_LDFLAGS = -module -avoid-version
librsyslog_la_LIBADD =  $(DL_LIBS) $(RT_LIBS)

#
# regular expression support
# 
if ENABLE_REGEXP
pkglib_LTLIBRARIES += lmregexp.la
lmregexp_la_SOURCES = regexp.c regexp.h
lmregexp_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmregexp_la_LDFLAGS = -module -avoid-version
lmregexp_la_LIBADD =
endif

if ENABLE_INET
pkglib_LTLIBRARIES += lmnet.la lmnetstrms.la
#
# network support
# 
lmnet_la_SOURCES = net.c net.h
lmnet_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmnet_la_LDFLAGS = -module -avoid-version
lmnet_la_LIBADD =

# network stream master class and stream factory
lmnetstrms_la_SOURCES = netstrms.c netstrms.h netstrm.c netstrm.h nssel.c nssel.h
lmnetstrms_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmnetstrms_la_LDFLAGS = -module -avoid-version
lmnetstrms_la_LIBADD =

# netstream drivers

# plain tcp driver - main driver
pkglib_LTLIBRARIES += lmnsd_ptcp.la
lmnsd_ptcp_la_SOURCES = nsd_ptcp.c nsd_ptcp.h nsdsel_ptcp.c nsdsel_ptcp.h
lmnsd_ptcp_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmnsd_ptcp_la_LDFLAGS = -module -avoid-version
lmnsd_ptcp_la_LIBADD =
endif # if ENABLE_INET

#
# GnuTLS netstream driver
# 
if ENABLE_GNUTLS
pkglib_LTLIBRARIES += lmnsd_gtls.la
lmnsd_gtls_la_SOURCES = nsd_gtls.c nsd_gtls.h nsdsel_gtls.c  nsdsel_gtls.h
lmnsd_gtls_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS) $(GNUTLS_CFLAGS)
lmnsd_gtls_la_LDFLAGS = -module -avoid-version
lmnsd_gtls_la_LIBADD = $(GNUTLS_LIBS)
endif

