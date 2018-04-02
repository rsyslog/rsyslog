sbin_PROGRAMS =
pkglib_LTLIBRARIES = 

pkgconfigdir = $(libdir)/pkgconfig

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
	README.md \
	platform/README \
	platform/freebsd/rsyslogd \
	platform/slackware/rc.rsyslogd \
	platform/redhat/rsyslog.conf \
	contrib/README \
	CONTRIBUTING.md \
	COPYING \
	COPYING.LESSER \
	COPYING.ASL20 \
	contrib/gnutls/ca.pem \
	contrib/gnutls/cert.pem \
	contrib/gnutls/key.pem \
	rsyslog.service.in

SUBDIRS = compat runtime grammar . plugins/immark plugins/imuxsock plugins/imtcp plugins/imudp plugins/omtesting
# external plugin driver is always enabled (core component)
SUBDIRS += plugins/mmexternal

if ENABLE_RSYSLOGD
SUBDIRS += tools
endif

if ENABLE_IMKLOG
SUBDIRS += plugins/imklog
endif

if ENABLE_IMKMSG
SUBDIRS += contrib/imkmsg
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

if ENABLE_OMFILE_HARDENED
SUBDIRS += contrib/omfile-hardened
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

if ENABLE_OMSTDOUT
SUBDIRS += plugins/omstdout
endif

if ENABLE_PMCISCONAMES
SUBDIRS += contrib/pmcisconames
endif

if ENABLE_PMCISCOIOS
SUBDIRS += plugins/pmciscoios
endif

if ENABLE_PMNULL
SUBDIRS += plugins/pmnull
endif

if ENABLE_PMNORMALIZE
SUBDIRS += plugins/pmnormalize
endif

if ENABLE_PMAIXFORWARDEDFROM
SUBDIRS += contrib/pmaixforwardedfrom
endif

if ENABLE_PMSNARE
SUBDIRS += contrib/pmsnare
endif

if ENABLE_PMPANNGFW
SUBDIRS += contrib/pmpanngfw
endif

if ENABLE_PMLASTMSG
SUBDIRS += plugins/pmlastmsg
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
SUBDIRS += contrib/omhiredis
endif

if ENABLE_OMZMQ3
SUBDIRS += contrib/omzmq3
endif

if ENABLE_OMCZMQ
SUBDIRS += contrib/omczmq
endif

if ENABLE_OMRABBITMQ
SUBDIRS += contrib/omrabbitmq
endif

if ENABLE_IMZMQ3
SUBDIRS += contrib/imzmq3
endif

if ENABLE_IMCZMQ
SUBDIRS += contrib/imczmq
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

if ENABLE_IMDIAG
SUBDIRS += plugins/imdiag
endif

if ENABLE_MAIL
SUBDIRS += plugins/ommail
endif

if ENABLE_FMHTTP
SUBDIRS += plugins/fmhttp
endif

if ENABLE_OMKAFKA
SUBDIRS += plugins/omkafka 
endif

if ENABLE_IMKAFKA
SUBDIRS += plugins/imkafka
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

if ENABLE_MMGROK
SUBDIRS += contrib/mmgrok
endif

if ENABLE_MMAUDIT
SUBDIRS += plugins/mmaudit
endif

if ENABLE_MMANON
SUBDIRS += plugins/mmanon
endif

if ENABLE_MMRM1STSPACE
SUBDIRS += plugins/mmrm1stspace
endif

if ENABLE_MMUTF8FIX
SUBDIRS += plugins/mmutf8fix
endif

if ENABLE_MMCOUNT
SUBDIRS += contrib/mmcount
endif

if ENABLE_MMSEQUENCE
SUBDIRS += contrib/mmsequence
endif

if ENABLE_MMDBLOOKUP
SUBDIRS += plugins/mmdblookup
endif


if ENABLE_MMFIELDS
SUBDIRS += plugins/mmfields
endif

if ENABLE_MMPSTRUCDATA
SUBDIRS += plugins/mmpstrucdata
endif

if ENABLE_MMRFC5424ADDHMAC
SUBDIRS += contrib/mmrfc5424addhmac
endif

# omhttpfs
if ENABLE_OMHTTPFS
SUBDIRS += contrib/omhttpfs
endif

# omamqp1
if ENABLE_OMAMQP1
SUBDIRS += contrib/omamqp1
endif

# omtcl
if ENABLE_OMTCL
SUBDIRS += contrib/omtcl
endif

# mmkubernetes
if ENABLE_MMKUBERNETES
SUBDIRS += contrib/mmkubernetes
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
DISTCHECK_CONFIGURE_FLAGS= \
	--disable-silent-rules \
	--disable-testbench \
	--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir) 

THIS_IS_TEMPORARILY_DISABLED = \
	--enable-distcheck-workaround \
	--enable-testbench \
	--enable-imdiag \
	--enable-testbench \
	--enable-imfile \
	--enable-snmp \
	--enable-libdbi \
	--enable-mysql \
	--enable-relp \
	--enable-rsyslogd \
	--enable-mail \
	--enable-klog \
	--enable-diagtools \
	--enable-mmgrok \
	--enable-gnutls \
	--enable-omstdout \
	--enable-pmlastmsg \
	--enable-omruleset \
	--enable-omprog \
	--enable-imptcp \
	--enable-omuxsock \
	--enable-impstats \
	--enable-memcheck \
	--enable-pmaixforwardedfrom \
	--enable-pmcisconames \
	--enable-pmsnare \
	--enable-elasticsearch \
	--enable-valgrind \
	--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir) 
# temporarily disable these checks for make distcheck 2012-09-06 rgerhards
#				--enable-mmsnmptrapd \
#				--enable-gssapi_krb5 \
#				--enable-extended-tests \
#				--enable-pgsql 

dist-hook:
	$(AM_V_GEN)echo $(VERSION) > $(distdir)/.tarball-version

ACLOCAL_AMFLAGS = -I m4
