# container for rsyslog development
# creates the build environment
FROM ubuntu:16.04
WORKDIR /home/devel
VOLUME /rsyslog
RUN groupadd rsyslog \
    && useradd -g rsyslog  -s /bin/bash rsyslog \
    && echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
ADD setup-system.sh setup-system.sh
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
    LD_LIBRARY_PATH=/usr/local/lib

# next ENV is specifically for running scan-build - so we do not need to
# change scripts if at a later time we can move on to a newer version
ENV SCAN_BUILD=scan-build \
    SCAN_BUILD_CC=clang-5.0

ENV RSYSLOG_CONFIGURE_OPTIONS --disable-generate-man-pages --enable-testbench --enable-imdiag --enable-imfile --enable-impstats --enable-mmrm1stspace --enable-imptcp --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omruleset --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-libgcrypt --enable-mmnormalize --enable-omudpspoof --enable-relp --enable-omrelp-default-port=13515 --enable-snmp --enable-mmsnmptrapd --enable-gnutls --enable-mysql --enable-mysql-tests --enable-ksi-ls12 --enable-libdbi --enable-pgsql --enable-pgsql-tests --enable-omhttpfs --enable-elasticsearch --enable-valgrind --enable-ommongodb --enable-omtcl --enable-mmdblookup --enable-mmcount --enable-gssapi-krb5 --enable-omhiredis --enable-imczmq --enable-omczmq --enable-usertools --enable-pmnull --enable-omhiredis --enable-imjournal --enable-omjournal --enable-omkafka=no --enable-imkafka=no --enable-mmgrok --enable-omamqp1 --enable-pmnormalize

# Install any needed packages
RUN ./setup-system.sh
WORKDIR /rsyslog
USER rsyslog
