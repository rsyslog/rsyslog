sbin_PROGRAMS =
pkglib_LTLIBRARIES = 

pkgconfigdir = $(libdir)/pkgconfig

if ENABLE_INET
pkglib_LTLIBRARIES += lmtcpsrv.la lmtcpclt.la
#
#
# TCP (stream) server support
#
lmtcpsrv_la_SOURCES = \
	tcps_sess.c \
	tcps_sess.h \
	tcpsrv.c \
	tcpsrv.h
lmtcpsrv_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmtcpsrv_la_LDFLAGS = -module -avoid-version
lmtcpsrv_la_LIBADD = 

#
# TCP (stream) client support
#
lmtcpclt_la_SOURCES = \
	tcpclt.c \
	tcpclt.h
lmtcpclt_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmtcpclt_la_LDFLAGS = -module -avoid-version
lmtcpclt_la_LIBADD = 

endif # if ENABLE_INET

#
# gssapi support
# 
if ENABLE_GSSAPI
pkglib_LTLIBRARIES += lmgssutil.la
lmgssutil_la_SOURCES = gss-misc.c gss-misc.h
lmgssutil_la_CPPFLAGS = $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
lmgssutil_la_LDFLAGS = -module -avoid-version
lmgssutil_la_LIBADD = $(GSS_LIBS)
endif

#
# systemd support
#
if HAVE_SYSTEMD

nodist_systemdsystemunit_DATA = \
	rsyslog.service

CLEANFILES = \
	rsyslog.service

%.service: %.service.in
	$(AM_V_GEN)sed -e 's,@sbindir\@,$(sbindir),g' $< > $@

endif

EXTRA_DIST = \
	platform/README \
	platform/freebsd/rsyslogd \
	platform/slackware/rc.rsyslogd \
	platform/redhat/rsyslog.conf \
	contrib/README \
	COPYING \
	COPYING.LESSER \
	COPYING.ASL20 \
	contrib/gnutls/ca.pem \
	contrib/gnutls/cert.pem \
	contrib/gnutls/key.pem \
	rsyslog.service.in

SUBDIRS = doc compat runtime grammar . plugins/immark plugins/imuxsock plugins/imtcp plugins/imudp plugins/omtesting

if ENABLE_RSYSLOGD
SUBDIRS += tools
endif

if ENABLE_IMKLOG
SUBDIRS += plugins/imklog
endif

if ENABLE_IMKMSG
SUBDIRS += plugins/imkmsg
endif

if ENABLE_IMPSTATS
SUBDIRS += plugins/impstats
endif

if ENABLE_IMSOLARIS
SUBDIRS += plugins/imsolaris
endif

if ENABLE_GSSAPI
SUBDIRS += plugins/omgssapi plugins/imgssapi
endif

if ENABLE_RELP
SUBDIRS += plugins/omrelp plugins/imrelp
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

if ENABLE_SMCUSTBINDCDR
SUBDIRS += plugins/sm_cust_bindcdr
endif

if ENABLE_OMSTDOUT
SUBDIRS += plugins/omstdout
endif

if ENABLE_PMCISCONAMES
SUBDIRS += plugins/pmcisconames
endif

if ENABLE_PMAIXFORWARDEDFROM
SUBDIRS += plugins/pmaixforwardedfrom
endif

if ENABLE_PMSNARE
SUBDIRS += plugins/pmsnare
endif

if ENABLE_PMLASTMSG
SUBDIRS += plugins/pmlastmsg
endif

if ENABLE_PMRFC3164SD
SUBDIRS += plugins/pmrfc3164sd
endif

if ENABLE_OMRULESET
SUBDIRS += plugins/omruleset
endif

if ENABLE_OMUDPSPOOF
SUBDIRS += plugins/omudpspoof
endif

if ENABLE_OMMONGODB
SUBDIRS += plugins/ommongodb
endif

if ENABLE_OMHIREDIS
SUBDIRS += plugins/omhiredis
endif

if ENABLE_OMZMQ3
SUBDIRS += plugins/omzmq3
endif

if ENABLE_OMRABBITMQ
SUBDIRS += plugins/omrabbitmq
endif

if ENABLE_IMZMQ3
SUBDIRS += plugins/imzmq3
endif

if ENABLE_OMUXSOCK
SUBDIRS += plugins/omuxsock
endif

if ENABLE_OMHDFS
SUBDIRS += plugins/omhdfs
endif

if ENABLE_OMJOURNAL
SUBDIRS += plugins/omjournal
endif

if ENABLE_IMJOURNAL
SUBDIRS += plugins/imjournal
endif

if ENABLE_ELASTICSEARCH
SUBDIRS += plugins/omelasticsearch
endif

if ENABLE_MMSNMPTRAPD
SUBDIRS += plugins/mmsnmptrapd
endif

if ENABLE_IMFILE
SUBDIRS += plugins/imfile
endif

if ENABLE_IMPTCP
SUBDIRS += plugins/imptcp
endif

if ENABLE_IMTTCP
SUBDIRS += plugins/imttcp
endif

if ENABLE_IMDIAG
SUBDIRS += plugins/imdiag
endif

if ENABLE_MAIL
SUBDIRS += plugins/ommail
endif

if ENABLE_OMPROG
SUBDIRS += plugins/omprog
endif

if ENABLE_RFC3195
SUBDIRS += plugins/im3195
endif

if ENABLE_MMNORMALIZE
SUBDIRS += plugins/mmnormalize
endif

if ENABLE_MMJSONPARSE
SUBDIRS += plugins/mmjsonparse
endif

if ENABLE_MMAUDIT
SUBDIRS += plugins/mmaudit
endif

if ENABLE_MMANON
SUBDIRS += plugins/mmanon
endif

if ENABLE_ORACLE
SUBDIRS += plugins/omoracle
endif

if ENABLE_GUI
SUBDIRS += java
endif

# tests are added as last element, because tests may need different
# modules that need to be generated first
SUBDIRS += tests


# make sure "make distcheck" tries to build all modules. This means that
# a developer must always have an environment where every supporting library
# is available. If that is not the case, the respective configure option may
# temporarily be removed below. The intent behind forcing everthing to compile
# in a make distcheck is so that we detect code that accidently was not updated
# when some global update happened.
DISTCHECK_CONFIGURE_FLAGS=	--enable-gssapi_krb5 \
				--enable-imfile \
				--enable-snmp \
				--enable-libdbi \
				--enable-mysql \
				--enable-relp \
				--enable-rsyslogd \
				--enable-mail \
				--enable-klog \
				--enable-diagtools \
				--enable-gnutls \
				--enable-omstdout \
				--enable-pmlastmsg \
				--enable-omruleset \
				--enable-omprog \
				--enable-imdiag \
				--enable-imptcp \
				--enable-imttcp \
				--enable-omuxsock \
				--enable-impstats \
				--enable-memcheck \
				--enable-pmaixforwardedfrom \
				--enable-pmcisconames \
				--enable-pmsnare \
				--enable-mmsnmptrapd \
				--enable-elasticsearch \
				--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir) 
# temporarily disable these checks for make distcheck 2012-09-06 rgerhards
#				--enable-extended-tests \
#				--enable-pgsql \
ACLOCAL_AMFLAGS = -I m4
