# container for rsyslog development
# creates the build environment
FROM ubuntu:18.04
ENV	DEBIAN_FRONTEND=noninteractive
RUN 	apt-get update && \
	apt-get upgrade -y
RUN	apt-get install -y \
	software-properties-common \
	wget \
	git \
	gcc \
	clang \
	autoconf \
	autotools-dev \
	automake \
	make \
	vim \
	gcc \
	mysql-server \
	pkg-config \
	libtool \
	python-docutils \
	pkg-config \
	libtool \
	gdb \
	valgrind \
	libdbi-dev \
	libsnmp-dev \
	libmysqlclient-dev \
	libglib2.0-dev \
	zlib1g-dev \
	uuid-dev \
	libgcrypt11-dev \
	bison \
	flex \
	clang \
	libcurl4-gnutls-dev \
	python-docutils  \
	wget \
	curl \
	libgnutls28-dev \
	libsystemd-dev \
	libnet1-dev \
	postgresql-client libpq-dev \
	libgrok1 libgrok-dev \
	libmongoc-dev \
	libbson-dev \
	git \
	faketime libdbd-mysql \
	autoconf-archive \
	libssl-dev libsasl2-dev \
	libmaxminddb-dev libmongoc-dev \
	liblz4-dev \
	libkrb5-dev \
	libhiredis-dev \
	libtokyocabinet-dev \
	default-jre
RUN	echo "deb http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_18.04/ ./" > /etc/apt/sources.list.d/0mq.list && \
	wget -nv -O - http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_18.04/Release.key | apt-key add - && \
 	add-apt-repository ppa:adiscon/v8-stable -y && \
	apt-get update -y
RUN	apt-get install -y  \
	libczmq-dev \
	libqpid-proton8-dev \
	tcl-dev \
	libsodium-dev \
	libestr-dev \
	librelp-dev \
	libfastjson-dev \
	liblogging-stdlog-dev \
	liblognorm-dev
WORKDIR /home/devel
VOLUME /rsyslog
RUN groupadd rsyslog \
    && useradd -g rsyslog  -s /bin/bash rsyslog \
    && echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
ADD setup-system.sh setup-system.sh
ENV	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
	LD_LIBRARY_PATH=/usr/local/lib \
	DEBIAN_FRONTEND=

# next ENV is specifically for running scan-build - so we do not need to
# change scripts if at a later time we can move on to a newer version
ENV SCAN_BUILD=scan-build \
    SCAN_BUILD_CC=clang-6.0

ENV RSYSLOG_CONFIGURE_OPTIONS --disable-generate-man-pages --enable-testbench --enable-imdiag --enable-imfile --enable-impstats --enable-mmrm1stspace --enable-imptcp --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omruleset --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-libgcrypt --enable-mmnormalize --enable-omudpspoof --enable-relp --enable-omrelp-default-port=13515 --enable-snmp --enable-mmsnmptrapd --enable-gnutls --enable-openssl --enable-mysql --enable-mysql-tests --enable-ksi-ls12 --enable-libdbi --enable-pgsql --enable-pgsql-tests --enable-omhttpfs --enable-elasticsearch --enable-valgrind --enable-ommongodb --enable-omtcl --enable-mmdblookup --enable-mmcount --enable-gssapi-krb5 --enable-omhiredis --enable-imczmq --enable-omczmq --enable-usertools --enable-pmnull --enable-omhiredis --enable-imjournal --enable-omjournal --enable-omkafka=no --enable-imkafka=no --enable-mmgrok --enable-omamqp1 --enable-pmnormalize --enable-mmkubernetes

# Install any needed packages
RUN ./setup-system.sh
WORKDIR /rsyslog
USER rsyslog
