sbin_PROGRAMS =
pkglib_LTLIBRARIES =

pkgconfigdir = $(libdir)/pkgconfig

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
	contrib/gnutls/key.pem

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

if ENABLE_OMSENDERTRACK
SUBDIRS += plugins/omsendertrack
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

if ENABLE_OMCZMQ
SUBDIRS += contrib/omczmq
endif

if ENABLE_OMRABBITMQ
SUBDIRS += contrib/omrabbitmq
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

if ENABLE_CLICKHOUSE
SUBDIRS += plugins/omclickhouse
endif

if ENABLE_OMHTTP
SUBDIRS += contrib/omhttp
endif

if ENABLE_SNMP
SUBDIRS += plugins/omsnmp
endif

if ENABLE_MMSNMPTRAPD
SUBDIRS += plugins/mmsnmptrapd
endif

if ENABLE_IMFILE
SUBDIRS += plugins/imfile
endif

if ENABLE_IMDOCKER
SUBDIRS += contrib/imdocker
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

if ENABLE_FMHASH
SUBDIRS += contrib/fmhash
endif

if ENABLE_FMUNFLATTEN
SUBDIRS += contrib/fmunflatten
endif

if ENABLE_FMPCRE
SUBDIRS += plugins/fmpcre
endif

if ENABLE_FFAUP
SUBDIRS += contrib/ffaup
endif

if ENABLE_OMKAFKA
SUBDIRS += plugins/omkafka
endif

if ENABLE_IMKAFKA
SUBDIRS += plugins/imkafka
endif

if ENABLE_OMAZUREEVENTHUBS
SUBDIRS += plugins/omazureeventhubs
endif

if ENABLE_IMDTLS
SUBDIRS += plugins/imdtls
endif

if ENABLE_OMDTLS
SUBDIRS += plugins/omdtls
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

if ENABLE_MMDARWIN
SUBDIRS += contrib/mmdarwin
endif

if ENABLE_MMFIELDS
SUBDIRS += plugins/mmfields
endif

if ENABLE_MMPSTRUCDATA
SUBDIRS += plugins/mmpstrucdata
endif

if ENABLE_MMAITAG
SUBDIRS += plugins/mmaitag
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

# imbatchreport
if ENABLE_IMBATCHREPORT
SUBDIRS += contrib/imbatchreport
endif

# omtcl
if ENABLE_OMTCL
SUBDIRS += contrib/omtcl
endif

# mmkubernetes
if ENABLE_MMKUBERNETES
SUBDIRS += contrib/mmkubernetes
endif

# impcap
if ENABLE_IMPCAP
SUBDIRS += contrib/impcap
endif

# imtuxedoulog
if ENABLE_IMTUXEDOULOG
SUBDIRS += contrib/imtuxedoulog
endif

# improg
if ENABLE_IMPROG
SUBDIRS += contrib/improg
endif

# imhttp
if ENABLE_IMHTTP
SUBDIRS += contrib/imhttp
endif

# mmtaghostname
if ENABLE_MMTAGHOSTNAME
SUBDIRS += contrib/mmtaghostname
endif

# imdb2diag
if ENABLE_PMDB2DIAG
SUBDIRS += contrib/pmdb2diag
endif

# imhiredis
if ENABLE_IMHIREDIS
SUBDIRS += contrib/imhiredis
endif

# tests are added as last element, because tests may need different
# modules that need to be generated first
SUBDIRS += tests



DISTCHECK_CONFIGURE_FLAGS=
# make sure "make distcheck" tries to build all modules. This means that
# a developer must always have an environment where every supporting library
# is available. If that is not the case, the respective configure option may
# temporarily be removed below. The intent behind forcing everthing to compile
# in a make distcheck is so that we detect code that accidently was not updated
# when some global update happened.
DISTCHECK_CONFIGURE_FLAGS+= \
	--enable-silent-rules \
	--enable-rsyslogd \
	--enable-omstdout \
	--enable-omsendertrack \
	--enable-imdiag \
	--enable-testbench \
	--enable-valgrind
# currently not supported in make distcheck:
# --enable-pgsql-tests
# --enable-extended-tests --> should probably never be enabled due to runtime

if ENABLE_DEFAULT_TESTS
DISTCHECK_CONFIGURE_FLAGS+= \
	--enable-diagtools \
	--enable-impstats \
	--enable-imptcp \
	--enable-klog \
	--enable-mail \
	--enable-mmanon \
	--enable-mmaudit \
	--enable-mmcount \
	--enable-mmexternal \
	--enable-mmfields \
	--enable-mmjsonparse \
	--enable-mmpstrucdata \
	--enable-mmrm1stspace \
	--enable-mmsequence \
	--enable-mmutf8fix \
	--enable-omruleset \
	--enable-omuxsock \
	--enable-pmaixforwardedfrom \
	--enable-pmciscoios \
	--enable-pmcisconames \
	--enable-pmlastmsg \
	--enable-pmnull \
	--enable-pmsnare \
	--enable-usertools

else
DISTCHECK_CONFIGURE_FLAGS+= \
	--disable-default-tests
endif  # if ENABLE_DEFAULT_TESTS

if ENABLE_IMPROG
DISTCHECK_CONFIGURE_FLAGS+= --enable-improg
endif

if ENABLE_IMHTTP
DISTCHECK_CONFIGURE_FLAGS+= --enable-imhttp
endif

if ENABLE_OMPROG
DISTCHECK_CONFIGURE_FLAGS+= --enable-omprog
endif

if ENABLE_GSSAPI
DISTCHECK_CONFIGURE_FLAGS+= --enable-gssapi-krb5
endif

if ENABLE_PMNORMALIZE
DISTCHECK_CONFIGURE_FLAGS+= --enable-pmnormalize
endif

if ENABLE_MMDBLOOKUP
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmdblookup
endif

if ENABLE_MMDARWIN
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmdarwin
endif

if ENABLE_MMNORMALIZE
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmnormalize
endif

if ENABLE_OMMONGODB
DISTCHECK_CONFIGURE_FLAGS+= --enable-ommongodb
endif

if ENABLE_OMHIREDIS
DISTCHECK_CONFIGURE_FLAGS+= --enable-omhiredis
endif

if ENABLE_MMSNMPTRAPD
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmsnmptrapd
endif

if ENABLE_OMLIBDBI
DISTCHECK_CONFIGURE_FLAGS+= --enable-libdbi
endif

if ENABLE_LIBGCRYPT
DISTCHECK_CONFIGURE_FLAGS+= --enable-libgcrypt
endif

if ENABLE_OMHTTP
DISTCHECK_CONFIGURE_FLAGS+= --enable-omhttp
endif

if ENABLE_OMHTTPFS
DISTCHECK_CONFIGURE_FLAGS+= --enable-omhttpfs
endif

if ENABLE_OMTCL
DISTCHECK_CONFIGURE_FLAGS+= --enable-omtcl
endif

if ENABLE_SNMP
DISTCHECK_CONFIGURE_FLAGS+= --enable-snmp
endif

if ENABLE_SNMP_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-snmp-tests
endif

if ENABLE_FMHTTP
DISTCHECK_CONFIGURE_FLAGS+= --enable-fmhttp
endif

if ENABLE_OMUDPSPOOF
DISTCHECK_CONFIGURE_FLAGS+= --enable-omudpspoof
endif

if ENABLE_PGSQL
DISTCHECK_CONFIGURE_FLAGS+= --enable-pgsql
endif

if ENABLE_GNUTLS
DISTCHECK_CONFIGURE_FLAGS+= --enable-gnutls
endif

if ENABLE_GNUTLS_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-gnutls-tests
else
DISTCHECK_CONFIGURE_FLAGS+= --disable-gnutls-tests
endif

if ENABLE_OPENSSL
DISTCHECK_CONFIGURE_FLAGS+= --enable-openssl
endif

if ENABLE_MYSQL
DISTCHECK_CONFIGURE_FLAGS+= --enable-mysql
endif
if ENABLE_MYSQL_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-mysql-tests
endif

if ENABLE_ELASTICSEARCH
DISTCHECK_CONFIGURE_FLAGS+= --enable-elasticsearch
endif
if ENABLE_ELASTICSEARCH_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-elasticsearch-tests
endif

if ENABLE_CLICKHOUSE
DISTCHECK_CONFIGURE_FLAGS+= --enable-clickhouse
endif
if ENABLE_CLICKHOUSE_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-clickhouse-tests
endif

if ENABLE_MMGROK
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmgrok
endif

if ENABLE_RELP
DISTCHECK_CONFIGURE_FLAGS+= --enable-relp --enable-omrelp-default-port=13515
endif

if ENABLE_IMKAFKA
DISTCHECK_CONFIGURE_FLAGS+= --enable-imkafka
endif
if ENABLE_OMKAFKA
DISTCHECK_CONFIGURE_FLAGS+= --enable-omkafka
endif
if ENABLE_KAFKA_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-kafka-tests
endif

if ENABLE_OMAZUREEVENTHUBS
DISTCHECK_CONFIGURE_FLAGS+= --enable-omazureeventhubs
endif

if ENABLE_IMDTLS
DISTCHECK_CONFIGURE_FLAGS+= --enable-imdtls
endif

if ENABLE_OMDTLS
DISTCHECK_CONFIGURE_FLAGS+= --enable-omdtls
endif

if ENABLE_IMJOURNAL
DISTCHECK_CONFIGURE_FLAGS+= --enable-imjournal
endif
if ENABLE_OMJOURNAL
DISTCHECK_CONFIGURE_FLAGS+= --enable-omjournal
endif
if ENABLE_JOURNAL_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-journal-tests
endif

if ENABLE_IMCZMQ
DISTCHECK_CONFIGURE_FLAGS+= --enable-imczmq
endif
if ENABLE_OMCZMQ
DISTCHECK_CONFIGURE_FLAGS+= --enable-omczmq
endif

if ENABLE_MMTAGHOSTNAME
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmtaghostname
endif

if ENABLE_IMTUXEDOULOG
DISTCHECK_CONFIGURE_FLAGS+= --enable-imtuxedolog
endif

if ENABLE_PMDB2DIAG
DISTCHECK_CONFIGURE_FLAGS+= --enable-pmdb2diag
endif

if ENABLE_IMBATCHREPORT
DISTCHECK_CONFIGURE_FLAGS+= --enable-imbatchreport
endif

if ENABLE_IMFILE
DISTCHECK_CONFIGURE_FLAGS+= --enable-imfile
endif

if ENABLE_IMFILE_TESTS
DISTCHECK_CONFIGURE_FLAGS+= --enable-imfile-tests
else
DISTCHECK_CONFIGURE_FLAGS+= --disable-imfile-tests
endif

if ENABLE_IMPCAP
DISTCHECK_CONFIGURE_FLAGS+= --enable-impcap
endif

if ENABLE_IMDOCKER
DISTCHECK_CONFIGURE_FLAGS+= --enable-imdocker
endif

if ENABLE_OMRABBITMQ
DISTCHECK_CONFIGURE_FLAGS+= --enable-omrabbitmq
endif

if ENABLE_MMKUBERNETES
DISTCHECK_CONFIGURE_FLAGS+= --enable-mmkubernetes
endif

if ENABLE_OMAMQP1
DISTCHECK_CONFIGURE_FLAGS+= --enable-omamqp1
endif

if ENABLE_DISTCHECK_WORKAROUND
DISTCHECK_CONFIGURE_FLAGS+= --disable-testbench
else
DISTCHECK_CONFIGURE_FLAGS+= --enable-testbench
endif

if ENABLE_IMHIREDIS
DISTCHECK_CONFIGURE_FLAGS+= --enable-imhiredis
endif

dist-hook:
	$(AM_V_GEN)echo $(VERSION) > $(distdir)/.tarball-version

ACLOCAL_AMFLAGS = -I m4
