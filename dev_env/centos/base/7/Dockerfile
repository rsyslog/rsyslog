# container for rsyslog development
# creates the build environment
# Note: this image currently uses in-container git checkouts to
# build the "rsyslog libraries" - we do not have packages for them
FROM centos:7
# search for packages that contain <file>: yum whatprovides <file>
RUN	yum -y update
RUN	yum -y install \
	autoconf \
	autoconf-archive \
	automake \
	bison \
	clang \
	clang-analyzer \
	compat-libstdc++-33 \
	curl \
	cyrus-sasl-devel \
	cyrus-sasl-lib \
	flex \
	gcc \
	gdb \
	git \
	gnutls-devel \
	hiredis-devel \
	java-1.8.0-openjdk \
	libcurl-devel \
	libdbi-dbd-mysql \
	libdbi-devel \
	libfaketime \
	libgcrypt-devel \
	libmaxminddb-devel \
	libnet libnet-devel \
	libtool \
	libuuid-devel \
	lsof \
	mysql-devel \
	nc \
	net-snmp-devel \
	openssl-devel \
	postgresql-devel \
	python-devel \
	python-docutils \
	python-sphinx \
	qpid-proton-c-devel \
	redhat-rpm-config \
	snappy-devel \
	sudo \
	systemd-devel \
	valgrind \
	wget \
	zlib-devel \
	# end of this RUN
RUN	yum -y install epel-release && \
	yum -y install \
	czmq-devel \
	hiredis \
	hiredis-devel \
	libmaxminddb \
	libmaxminddb-devel \
	mongo-c-driver \
	mongo-c-driver-devel \
	# end of this RUN
WORKDIR /home/devel
VOLUME /rsyslog
RUN	groupadd rsyslog \
	&& adduser -g rsyslog  -s /bin/bash rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
ADD setup-system.sh setup-system.sh
ADD common/setup-projects.sh setup-projects.sh
ENV	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
	LD_LIBRARY_PATH=/usr/local/lib \
	LIBDIR_PATH=/usr/lib64

# next ENV is specifically for running scan-build - so we do not need to
# change scripts if at a later time we can move on to a newer version
#ENV SCAN_BUILD=scan-build \
#    SCAN_BUILD_CC=clang-5.0

ENV RSYSLOG_CONFIGURE_OPTIONS --disable-generate-man-pages --enable-testbench --enable-imdiag --enable-imfile --enable-impstats --enable-mmrm1stspace --enable-imptcp --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omruleset --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-libgcrypt --enable-mmnormalize --enable-omudpspoof --enable-relp --enable-omrelp-default-port=13515 --enable-snmp --enable-mmsnmptrapd --enable-gnutls --enable-openssl --enable-mysql --enable-mysql-tests --enable-libdbi --enable-pgsql --enable-pgsql-tests --enable-omhttpfs --enable-elasticsearch --enable-valgrind --enable-ommongodb --enable-mmdblookup --enable-mmcount --enable-gssapi-krb5 --enable-omhiredis --enable-imczmq --enable-omczmq --enable-usertools --enable-pmnull --enable-omhiredis --enable-imjournal --enable-omjournal --enable-omkafka --enable-imkafka --enable-omamqp1 --enable-pmnormalize --enable-ksi-ls12=no --enable-omtcl=no --enable-mmgrok=no 

# Install any needed packages
RUN ./setup-system.sh
RUN ./setup-projects.sh
WORKDIR /rsyslog
USER rsyslog
RUN	sudo yum -y install \
	java-1.8.0-openjdk-devel \
	net-tools
