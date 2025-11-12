#!/bin/bash
set -e
set -v

export CC=clang #gcc # clang --> package currently broken in fedora 23
export CFLAGS="-g" # -fsanitize=address"
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"
autoreconf -fiv
./configure --prefix=/usr/local --mandir=/usr/share/man --infodir=/usr/share/info --datadir=/usr/share --sysconfdir=/etc --localstatedir=/var/lib --disable-dependency-tracking --disable-silent-rules --docdir=/usr/share/doc/rsyslog --disable-generate-man-pages --enable-testbench --enable-imdiag --enable-imfile --enable-impstats --enable-imptcp --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmjsontransform --enable-mmjsonrewrite --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omruleset --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-libgcrypt --enable-mmnormalize --disable-omudpspoof --enable-relp --disable-snmp --disable-mmsnmptrapd --enable-gnutls --enable-usertools=no --enable-mysql --enable-valgrind --enable-omjournal
make clean
# we don't want to run the testbench, but we want to build testbench tools
make check TESTS=""
