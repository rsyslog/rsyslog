# container for rsyslog development
# creates the build environment
FROM	ubuntu:19.04
ENV	DEBIAN_FRONTEND=noninteractive
RUN 	apt-get update && \
	apt-get upgrade -y && \
	apt-get install -y \
	autoconf \
	autoconf-archive \
	automake \
	autotools-dev \
	bison \
	clang \
	clang-tools \
	curl \
	default-jdk \
	default-jre \
	faketime libdbd-mysql \
	flex \
	gcc \
	gcc-8 \
	gdb \
	git \
	libbson-dev \
	libcurl4-gnutls-dev \
	libdbi-dev \
	libgcrypt11-dev \
	libglib2.0-dev \
	libgnutls28-dev \
	libgrok1 libgrok-dev \
	libhiredis-dev \
	libkrb5-dev \
	liblz4-dev \
	libmaxminddb-dev libmongoc-dev \
	libmongoc-dev \
	libmysqlclient-dev \
	libnet1-dev \
	libpcap-dev \
	librabbitmq-dev \
	libsnmp-dev \
	libssl-dev libsasl2-dev \
	libsystemd-dev \
	libtokyocabinet-dev \
	libtool \
	libtool-bin \
	logrotate \
	lsof \
	make \
	mysql-server \
	net-tools \
	pkg-config \
	postgresql-client libpq-dev \
	python-docutils  \
	python-pip \
	python-pysnmp4 \
	snmp-mibs-downloader \
	software-properties-common \
	sudo \
	uuid-dev \
	valgrind \
	vim \
	wget \
	zlib1g-dev
ENV	REBUILD=1
# Adiscon/rsyslog components
RUN	apt-key adv --keyserver keyserver.ubuntu.com --recv E0C56BD4 && \
 	add-apt-repository ppa:adiscon/v8-stable -y && \
	apt-get update -y && \
	apt-get install -y  \
	libestr-dev \
	librelp-dev \
	libfastjson-dev \
	liblogging-stdlog-dev \
	liblognorm-dev
# 0mq (currently not needed, but we keep it in just in case)
#RUN	echo "deb http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_18.04/ ./" > /etc/apt/sources.list.d/0mq.list && \
#	wget -nv -O - http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_18.04/Release.key | apt-key add - && \
#	echo "deb http://repo.yandex.ru/clickhouse/deb/stable/ main/" > /etc/apt/sources.list.d/clickhouse.list && \
RUN	apt-get update -y && \
	apt-get install -y  \
	libczmq-dev \
	libqpid-proton11-dev \
	tcl-dev \
	libsodium-dev
# clickhouse
#RUN	echo "deb http://repo.yandex.ru/clickhouse/deb/stable/ main/" > /etc/apt/sources.list.d/clickhouse.list && \
#	apt-key adv --keyserver keyserver.ubuntu.com --recv E0C56BD4 && \
# 	add-apt-repository ppa:adiscon/v8-stable -y && \
#	apt-get update -y && \
#	apt-get install -y  \
#	clickhouse-client \
#	clickhouse-server

# clang devel version
RUN	echo "deb http://apt.llvm.org/disco/ llvm-toolchain-disco-9 main" > /etc/apt/sources.list.d/llvm8.list && \
	echo "deb-src http://apt.llvm.org/disco/ llvm-toolchain-disco-9 main" >> /etc/apt/sources.list.d/llvm8.list && \
	wget -nv -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
	apt-get update -y && \
	apt-get install -y \
	clang-9 \
	clang-tools-9 \
	lldb-9 \
	lld-9
RUN	echo "deb http://apt.llvm.org/disco/ llvm-toolchain-disco main" > /etc/apt/sources.list.d/llvm10.list && \
	echo "deb-src http://apt.llvm.org/disco/ llvm-toolchain-disco main" >> /etc/apt/sources.list.d/llvm10.list && \
	wget -nv -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
	apt-get update -y && \
	apt-get install -y clang-10 clang-tools-10 clang-10-doc libclang-common-10-dev libclang-10-dev libclang1-10 clang-format-10 python-clang-10 clangd-10

# create dependency cache
RUN	mkdir /local_dep_cache && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-5.6.9.tar.gz -O /local_dep_cache/elasticsearch-5.6.9.tar.gz && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-6.0.0.tar.gz -O /local_dep_cache/elasticsearch-6.0.0.tar.gz && \
	wget -nv https://www-us.apache.org/dist/zookeeper/zookeeper-3.4.14/zookeeper-3.4.14.tar.gz -O /local_dep_cache/zookeeper-3.4.14.tar.gz && \
	wget -nv http://www-us.apache.org/dist/kafka/2.2.0/kafka_2.12-2.2.0.tgz -O /local_dep_cache/kafka_2.12-2.2.0.tgz
# tell tests which are the newester versions, so they can be checked without the need
# to adjust test sources.
#ENV	ELASTICSEARCH_NEWEST="elasticsearch-6.3.1.tar.gz"


# for clickhouse, the container requires some pre-generated files for TLS. generate them via
# openssl req -subj "/CN=localhost" -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout clickhouse.server.key -out clickhouse.server.crt
# openssl dhparam -out clickhouse.dhparam.pem 2048
#COPY	clickhouse.dhparam.pem /etc/clickhouse-server/dhparam.pem
#COPY	clickhouse.server.crt /etc/clickhouse-server/server.crt
#COPY	clickhouse.server.key /etc/clickhouse-server/server.key
#RUN	sed -i 's/<yandex>/<yandex>\n    <core_dump><size_limit>0<\/size_limit><\/core_dump>/g' \
#		/etc/clickhouse-server/config.xml && \
#	sed -i 's/<tcp_port>9000<\/tcp_port>/<tcp_port>9000<\/tcp_port>\n    <https_port>8443<\/https_port>/g' \
#		/etc/clickhouse-server/config.xml

WORKDIR	/home/devel
VOLUME	/rsyslog
RUN	groupadd rsyslog \
	&& useradd -g rsyslog  -s /bin/bash rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers \
	&& echo "buildbot ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# mysql needs a little help:
RUN	mkdir -p /var/run/mysqld && \
	chown mysql:mysql /var/run/mysqld
ENV	MYSQLD_START_CMD="sudo mysqld_safe" \
        MYSQLD_STOP_CMD="sudo kill $(sudo cat /var/run/mysqld/mysqld.pid)"

# and so does clickhouse
#RUN	chown root:root /var/lib/clickhouse
#ENV	CLICKHOUSE_START_CMD="sudo clickhouse-server --config-file=/etc/clickhouse-server/config.xml" \
#	CLICKHOUSE_STOP_CMD="sudo kill $(pidof clickhouse-server)"

ADD	setup-system.sh setup-system.sh
ENV	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
	LD_LIBRARY_PATH=/usr/local/lib \
	DEBIAN_FRONTEND= \
	SUDO="sudo -S"

# Install any needed packages
RUN	./setup-system.sh

# other manual installs
# kafkacat
RUN	cd helper-projects \
	&& git clone https://github.com/edenhill/kafkacat \
	&& cd kafkacat \
	&& (unset CFLAGS; ./configure --prefix=/usr --CFLAGS="-g" ; make -j2) \
	&& make install \
	&& cd .. \
	&& cd ..
# Note: we do NOT delete the source as we may need it to
# uninstall (in case the user wants to go back to system-default)

# next ENV is specifically for running scan-build - so we do not need to
# change scripts if at a later time we can move on to a newer version
ENV	SCAN_BUILD=scan-build-9 \
	SCAN_BUILD_CC=clang-9 \
	ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-9/bin/llvm-symbolizer

ENV RSYSLOG_CONFIGURE_OPTIONS \
	--enable-compile-warning=error \
	--enable-clickhouse \
	--enable-clickhouse-tests=no \
	--enable-elasticsearch \
	--enable-elasticsearch-tests \
	--enable-gnutls \
	--enable-gssapi-krb5 \
	--enable-imbatchreport \
	--enable-imczmq \
	--enable-imdiag \
	--enable-imdocker \
	--enable-imdocker-tests \
	--enable-imfile \
	--enable-imjournal \
	--enable-imkafka \
	--enable-impstats \
	--enable-impcap \
	--enable-imptcp \
	--enable-kafka-tests \
	--enable-ksi-ls12 \
	--enable-libdbi \
	--enable-libfaketime \
	--enable-libgcrypt \
	--enable-mail \
	--enable-mmanon \
	--enable-mmaudit \
	--enable-mmcapture \
	--enable-mmcount \
	--enable-mmdarwin \
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
	--enable-mysql-tests \
	--enable-omamqp1 \
	--enable-omczmq \
	--enable-omhiredis \
	--enable-omhiredis \
	--enable-omhttpfs \
	--enable-omhttp \
	--enable-omjournal \
	--enable-omkafka \
	--enable-ommongodb \
	--enable-omprog \
	--enable-omrabbitmq \
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
	--enable-snmp-tests \
	--enable-usertools \
	--enable-valgrind \
	\
	--enable-testbench

# module needs fixes: --enable-kmsg
RUN	printf '\n' > /var/log/mysql/error.log
VOLUME	/var/lib/mysql
WORKDIR	/rsyslog
USER	rsyslog
