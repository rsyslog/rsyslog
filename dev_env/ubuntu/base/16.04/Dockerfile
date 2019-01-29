# container for rsyslog development
# creates the build environment
FROM	ubuntu:16.04
ENV	DEBIAN_FRONTEND=noninteractive
RUN 	apt-get update && \
	apt-get upgrade -y
RUN	apt-get install -y \
	autoconf \
	autoconf-archive \
	automake \
	autotools-dev \
	bison \
	clang \
	curl \
	default-jdk \
	faketime \
	flex \
	gdb \
	git \
	libbson-dev \
	libcurl4-gnutls-dev \
	libdbd-mysql \
	libdbi-dev \
	libgcrypt11-dev \
	libglib2.0-dev \
	libgnutls28-dev \
	libgrok1 libgrok-dev \
	libhiredis-dev \
	libkrb5-dev \
	liblz4-dev \
	libmaxminddb-dev \
	libmongoc-dev \
	libmysqlclient-dev \
	libnet1-dev \
	libpcap-dev \
	libpq-dev \
	libsasl2-dev \
	libsnmp-dev \
	libssl-dev \
	libsystemd-dev \
	libtokyocabinet-dev \
	libtool \
	mysql-server \
	net-tools \
	pkg-config \
	postgresql-client \
	python-docutils \
	python-software-properties \
	software-properties-common \
	sudo \
	tcl-dev \
	uuid-dev \
	valgrind \
	vim \
	wget \
	zlib1g-dev
RUN     add-apt-repository ppa:adiscon/v8-stable -y && \
	add-apt-repository ppa:qpid/released -y && \
	add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
	echo "deb http://repo.yandex.ru/clickhouse/deb/stable/ main/" > /etc/apt/sources.list.d/clickhouse.list && \
	apt-key adv --keyserver keyserver.ubuntu.com --recv E0C56BD4 && \
	echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-5.0 main" > /etc/apt/sources.list.d/llvm.list && \
	echo "deb http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_16.04/ ./" > /etc/apt/sources.list.d/0mq.list && \
	wget -O - http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_16.04/Release.key | apt-key add - && \
	wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key| apt-key add -
# note: ppa:ubuntu-toolchain-r/test is currently the best repo for gcc-7 we can find...

RUN	apt-get update -y && \
	apt-get install -y \
	clickhouse-client \
	clickhouse-server \
	libestr-dev \
	librelp-dev \
	libqpid-proton10-dev \
	libsodium-dev \
	libfastjson-dev \
	liblogging-stdlog-dev \
	gcc-7 \
	libczmq-dev \
	clang-5.0 \
	clang-tools-5.0 \
	liblognorm-dev

# for clickhouse, the container requires some pre-generated files for TLS. generate them via
# openssl req -subj "/CN=localhost" -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout clickhouse.server.key -out clickhouse.server.crt
# openssl dhparam -out clickhouse.dhparam.pem 2048
COPY	clickhouse.dhparam.pem /etc/clickhouse-server/dhparam.pem
COPY	clickhouse.server.crt /etc/clickhouse-server/server.crt
COPY	clickhouse.server.key /etc/clickhouse-server/server.key
RUN	sed -i 's/<yandex>/<yandex>\n    <core_dump><size_limit>0<\/size_limit><\/core_dump>/g' \
		/etc/clickhouse-server/config.xml && \
	sed -i 's/<tcp_port>9000<\/tcp_port>/<tcp_port>9000<\/tcp_port>\n    <https_port>8443<\/https_port>/g' \
		/etc/clickhouse-server/config.xml

WORKDIR	/home/devel
VOLUME	/rsyslog
RUN	groupadd rsyslog \
	&& useradd -g rsyslog  -s /bin/bash rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers \
	&& echo "buildbot ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
ADD	setup-system.sh setup-system.sh
ENV	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
	LD_LIBRARY_PATH=/usr/local/lib \
	DEBIAN_FRONTEND=

# Install any needed packages
RUN	./setup-system.sh

# create dependency cache
RUN	mkdir /local_dep_cache && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-5.6.9.tar.gz -O /local_dep_cache/elasticsearch-5.6.9.tar.gz  && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-6.0.0.tar.gz -O /local_dep_cache/elasticsearch-6.0.0.tar.gz && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-6.3.1.tar.gz -O /local_dep_cache/elasticsearch-6.3.1.tar.gz
# tell tests which are the newester versions, so they can be checked without the need
# to adjust test sources.
ENV	ELASTICSEARCH_NEWEST="elasticsearch-6.3.1.tar.gz"

# tell CI env how to handle clickhouse
RUN	chown root:root /var/lib/clickhouse
ENV	CLICKHOUSE_START_CMD="sudo -S clickhouse-server --config-file=/etc/clickhouse-server/config.xml" \
	CLICKHOUSE_STOP_CMD="sudo -S kill $(pidof clickhouse-server)"

# next ENV is specifically for running scan-build - so we do not need to
# change scripts if at a later time we can move on to a newer version
ENV SCAN_BUILD=scan-build \
    SCAN_BUILD_CC=clang-5.0

ENV RSYSLOG_CONFIGURE_OPTIONS \
	--enable-clickhouse \
	--enable-clickhouse-tests \
	--enable-elasticsearch \
	--enable-elasticsearch-tests \
	--enable-gnutls \
	--enable-gssapi-krb5 \
	--enable-imczmq \
	--enable-imdiag \
	--enable-imfile \
	--enable-imjournal \
	--enable-imkafka \
	--enable-impcap \
	--enable-impstats \
	--enable-imptcp \
	--enable-kafka-tests \
	--disable-kmsg \
	--enable-ksi-ls12 \
	--enable-libdbi \
	--enable-libfaketime \
	--enable-libgcrypt \
	--enable-mail \
	--enable-mmanon \
	--enable-mmaudit \
	--enable-mmcapture \
	--enable-mmcapture \
	--enable-mmcount \
	--enable-mmdblookup \
	--enable-mmfields \
	--enable-mmgrok \
	--enable-mmjsonparse \
	--enable-mmkubernetes \
	--enable-mmnormalize \
	--enable-mmpstrucdata \
	--enable-mmrm1stspace \
	--enable-mmsequence \
	--enable-mmsnmptrapd \
	--enable-mmutf8fix \
	--enable-mysql \
	--enable-omamqp1 \
	--enable-omczmq \
	--enable-omhiredis \
	--enable-omhttp \
	--enable-omhttp \
	--enable-omhttpfs \
	--enable-omjournal \
	--enable-omkafka \
	--enable-ommongodb \
	--enable-omprog \
	--enable-omrelp-default-port=13515 \
	--enable-omruleset \
	--enable-omstdout \
	--enable-omtcl \
	--enable-omudpspoof \
	--enable-omuxsock \
	--enable-openssl \
	--enable-pgsql \
	--enable-pmaixforwardedfrom \
	--enable-pmciscoios \
	--enable-pmcisconames \
	--enable-pmlastmsg \
	--enable-pmnormalize \
	--enable-pmnull \
	--enable-pmsnare \
	--enable-relp \
	--enable-snmp \
	--enable-usertools \
	--enable-valgrind \
	\
	--enable-testbench

WORKDIR /rsyslog
USER rsyslog
