sbin_PROGRAMS =
pkglib_LTLIBRARIES = 


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

EXTRA_DIST = \
	freebsd/rsyslogd \
	slackware/rc.rsyslogd \
	contrib/README \
	rsyslog.conf \
	COPYING.LESSER \
	contrib/gnutls/ca.pem \
	contrib/gnutls/cert.pem \
	contrib/gnutls/key.pem

SUBDIRS = doc runtime . plugins/immark plugins/imuxsock plugins/imtcp plugins/imudp plugins/omtesting

if ENABLE_RSYSLOGD
SUBDIRS += tools
endif

if ENABLE_IMKLOG
SUBDIRS += plugins/imklog
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

if ENABLE_IMTEMPLATE
SUBDIRS += plugins/imtemplate
endif

if ENABLE_OMSTDOUT
SUBDIRS += plugins/omstdout
endif

if ENABLE_OMTEMPLATE
SUBDIRS += plugins/omtemplate
endif

if ENABLE_IMFILE
SUBDIRS += plugins/imfile
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

# tests are added as last element, because tests may need different
# modules that need to be generated first
SUBDIRS += tests

# make sure "make distcheck" tries to build all modules. This means that
# a developer must always have an environment where every supporting library
# is available. If that is not the case, the respective configure option may
# temporarily be removed below. The intent behind forcing everthing to compile
# in a make distcheck is so that we detect code that accidently was not updated
# when some global update happened.
DISTCHECK_CONFIGURE_FLAGS=--enable-gssapi_krb5 --enable-imfile --enable-snmp --enable-pgsql --enable-libdbi --enable-mysql --enable-omtemplate --enable-imtemplate --enable-relp --enable-rsyslogd --enable-mail --enable-klog  --enable-diagtools --enable-gnutls --enable-omstdout --enable-omprog
ACLOCAL_AMFLAGS = -I m4
