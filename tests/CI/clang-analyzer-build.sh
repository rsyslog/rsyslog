#!/bin/bash
#set -e
#set -v

export CC=clang-4.0
export SCANBUILD=scan-build-4.0
#export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"
#autoreconf -fiv
#scan-build-4.0 ./configure --prefix=/usr/local --infodir=/usr/share/info --datadir=/usr/share --sysconfdir=/etc --localstatedir=/var/lib --disable-dependency-tracking --docdir=/usr/share/doc/rsyslog ...
$SCANBUILD ./configure --disable-generate-man-pages --enable-debug --enable-testbench --enable-imdiag --enable-imfile --enable-impstats --enable-imptcp --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omruleset --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-libgcrypt --enable-mmnormalize --disable-omudpspoof --enable-relp --disable-snmp --disable-mmsnmptrapd --enable-gnutls --enable-usertools=no --enable-mysql --enable-valgrind --enable-omjournal --enable-omczmq  --enable-imczmq
make clean
# we don't want to run the testbench, but we want to build testbench tools
# make check currently produces issues
#scan-build-4.0 make check TESTS="" -j
rm -r /tmp/scan-build-results
rm analyzer-result.tar.gz
$SCANBUILD -o /tmp/scan-build-results --status-bugs make -j4
RESULT=$?
echo scan-build result: $RESULT
tar cf analyzer-result.tar.gz /tmp/scan-build-results
rm -r /tmp/scan-build-results
exit $RESULT
