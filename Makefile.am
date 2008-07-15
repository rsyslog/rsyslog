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
lmtcpsrv_la_CPPFLAGS = $(pthreads_cflags) $(rsrt_cflags)
lmtcpsrv_la_LDFLAGS = -module -avoid-version
lmtcpsrv_la_LIBADD = 

#
# TCP (stream) client support
#
lmtcpclt_la_SOURCES = \
	tcpclt.c \
	tcpclt.h
lmtcpclt_la_CPPFLAGS = $(pthreads_cflags) $(rsrt_cflags)
lmtcpclt_la_LDFLAGS = -module -avoid-version
lmtcpclt_la_LIBADD = 

endif # if ENABLE_INET

#
# gssapi support
# 
if ENABLE_GSSAPI
pkglib_LTLIBRARIES += lmgssutil.la
lmgssutil_la_SOURCES = gss-misc.c gss-misc.h
lmgssutil_la_CPPFLAGS = $(pthreads_cflags) $(rsrt_cflags)
lmgssutil_la_LDFLAGS = -module -avoid-version
lmgssutil_la_LIBADD = $(gss_libs)
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

SUBDIRS = doc runtime . tests

SUBDIRS += plugins/immark plugins/imuxsock plugins/imtcp plugins/imudp plugins/omtesting

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

if ENABLE_IMFILE
SUBDIRS += plugins/imfile
endif

if ENABLE_MAIL
SUBDIRS += plugins/ommail
endif

if ENABLE_RFC3195
SUBDIRS += plugins/im3195
endif
