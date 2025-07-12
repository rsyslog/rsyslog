# container for rsyslog development
# creates the build environment
FROM ubuntu:devel
WORKDIR /home/ci
ADD common/setup-projects.sh /home/ci
ADD setup-system.sh setup-system.sh
ENV SUDO=
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
    LD_LIBRARY_PATH=/usr/local/lib
ENV RSYSLOG_CONFIGURE_OPTIONS -enable-testbench --enable-imdiag --enable-imfile --enable-impstats --enable-imptcp --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omruleset --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-libgcrypt --enable-mmnormalize --enable-omudpspoof --enable-relp --enable-snmp --enable-gnutls --enable-mysql --enable-usertools --enable-libdbi --enable-omhttpfs --enable-elasticsearch --enable-valgrind --enable-ommongodb --enable-imjournal --enable-omjournal --enable-omamqp1=no --enable-omczmq --enable-imzcmq

# not compatible with Container in it's current form:
# --enable-mysql-tests

# Install any needed packages
RUN ./setup-system.sh
# the second step shall later go into a different container
# for now, let's get things going first...
RUN ./setup-projects.sh
USER 1000:1000
