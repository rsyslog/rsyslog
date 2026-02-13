# container for rsyslog development
# creates the build environment
# Note: platform not suitable for zstd, distro-version is too old
FROM	ubuntu:18.04
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
	iproute2 \
	libaprutil1-dev \
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
	libprotobuf-c-dev \
	librabbitmq-dev \
	libsnappy-dev \
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
	protobuf-c-compiler \
	software-properties-common \
	sudo \
	uuid-dev \
	valgrind \
	vim \
	wget \
	zlib1g-dev 
ENV	REBUILD=3
# Adiscon/rsyslog components
RUN	apt-key adv --keyserver keyserver.ubuntu.com --recv E0C56BD4 && \
 	add-apt-repository ppa:adiscon/v8-stable -y && \
	apt-get update -y && \
	apt-get install -y  \
	libestr-dev \
	librelp-dev \
	liblogging-stdlog-dev \
	liblognorm-dev
# 0mq (currently not needed, but we keep it in just in case)
#RUN	echo "deb http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_18.04/ ./" > /etc/apt/sources.list.d/0mq.list && \
#	wget -nv -O - http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_18.04/Release.key | apt-key add - && \
#	echo "deb http://repo.yandex.ru/clickhouse/deb/stable/ main/" > /etc/apt/sources.list.d/clickhouse.list && \
RUN	apt-get update -y && \
	apt-get install -y  \
	libczmq-dev \
	libqpid-proton8-dev \
	tcl-dev \
	libsodium-dev
# clickhouse
RUN	echo "deb https://packages.clickhouse.com/deb stable main" > /etc/apt/sources.list.d/clickhouse.list && \
	apt-key adv --keyserver keyserver.ubuntu.com --recv E0C56BD4 && \
	apt-key adv --keyserver keyserver.ubuntu.com --recv 3E4AD4719DDE9A38 && \
 	add-apt-repository ppa:adiscon/v8-stable -y && \
	apt-get update -y && \
	apt-get install -y  \
	clickhouse-client \
	clickhouse-server
# clang devel version
RUN	echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main" > /etc/apt/sources.list.d/llvm8.list && \
	echo "deb-src http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main" >> /etc/apt/sources.list.d/llvm8.list && \
	wget -nv -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
	apt-get update -y && \
	apt-get install -y \
	clang-8 \
	lldb-8 \
	lld-8 # version 9 currently has a conflict!

# create dependency cache
RUN	mkdir /local_dep_cache
RUN	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-6.0.0.tar.gz -O /local_dep_cache/elasticsearch-6.0.0.tar.gz
RUN	wget -nv https://www.rsyslog.com/files/download/rsyslog/elasticsearch-7.14.1-linux-x86_64.tar.gz -O /local_dep_cache/elasticsearch-7.14.1-linux-x86_64.tar.gz
RUN	wget -nv https://archive.apache.org/dist/zookeeper/zookeeper-3.9.3/apache-zookeeper-3.9.3-bin.tar.gz -O /local_dep_cache/apache-zookeeper-3.9.3-bin.tar.gz
RUN	wget -nv https://www.apache.org/dyn/closer.cgi?path=/kafka/2.8.0/kafka_2.13-2.8.0.tgz -O /local_dep_cache/kafka_2.13-2.8.0.tgz

# tell tests which are the newester versions, so they can be checked without the need
# to adjust test sources.
#ENV	ELASTICSEARCH_NEWEST="elasticsearch-6.3.1.tar.gz"


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

# we need this for sudo to work...
RUN	echo 'user1001:x:1001:1001::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1002:x:1002:1002::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1003:x:1003:1003::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1004:x:1004:1004::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1005:x:1005:1005::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1006:x:1006:1006::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1007:x:1007:1007::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1008:x:1008:1008::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1009:x:1009:1009::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1010:x:1010:1010::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'grp1001:x:1001:user1001' >> /etc/group && \
	echo 'grp1002:x:1002:user1002' >> /etc/group && \
	echo 'grp1003:x:1003:user1003' >> /etc/group && \
	echo 'grp1004:x:1004:user1004' >> /etc/group && \
	echo 'grp1005:x:1005:user1005' >> /etc/group && \
	echo 'grp1006:x:1006:user1006' >> /etc/group && \
	echo 'grp1007:x:1007:user1007' >> /etc/group && \
	echo 'grp1008:x:1008:user1008' >> /etc/group && \
	echo 'grp1009:x:1009:user1009' >> /etc/group && \
	echo 'grp1010:x:1010:user1010' >> /etc/group && \
	echo "user1001 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	echo "user1002 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	echo "user1003 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	echo "user1004 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	echo "user1005 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	echo "user1006 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	echo "user1007 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	echo "user1008 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	echo "user1009 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	echo "user1010 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# mysql needs a little help:
RUN	mkdir -p /var/run/mysqld && \
	chown mysql:mysql /var/run/mysqld
ENV	MYSQLD_START_CMD="sudo mysqld_safe" \
        MYSQLD_STOP_CMD="sudo kill $(sudo cat /var/run/mysqld/mysqld.pid)"

# and so does clickhouse
RUN	chown root:root /var/lib/clickhouse
ENV	CLICKHOUSE_START_CMD="sudo clickhouse-server --config-file=/etc/clickhouse-server/config.xml" \
	CLICKHOUSE_STOP_CMD="sudo kill $(pidof clickhouse-server)"

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
	&& (unset CFLAGS; ./configure --prefix=/usr --CFLAGS="-g" ; make -j) \
	&& make install \
	&& cd ..
# Note: we do NOT delete the source as we may need it to
# uninstall (in case the user wants to go back to system-default)

# we need civetweb, as there are no packages for it
RUN	cd helper-projects && \
	git clone https://github.com/civetweb/civetweb.git \
	&& cd civetweb \
	&& (unset CFLAGS; make -j build COPT="-DREENTRANT_TIME"; make install-headers ; make install-slib ) \
	&& cd .. \
	&& rm -rf civetweb

RUN	cd ..

ENV	REBUILD_HERE=1
# liblfastjson
RUN	cd helper-projects && \
	git clone https://github.com/rsyslog/libfastjson.git && \
	cd libfastjson && \
	autoreconf -fi && \
	./configure --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu --includedir=/usr/include && \
	make -j && \
	make install && \
	cd .. && \
	rm -r libfastjson && \
	cd ..

# next ENV is specifically for running scan-build - so we do not need to
# change scripts if at a later time we can move on to a newer version
ENV	SCAN_BUILD=scan-build \
	SCAN_BUILD_CC=clang-8 \
	ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-8/bin/llvm-symbolizer

ENV RSYSLOG_CONFIGURE_OPTIONS \
	--enable-clickhouse \
	--enable-clickhouse-tests \
	--enable-elasticsearch \
	--enable-elasticsearch-tests \
	--enable-gnutls \
	--enable-gssapi-krb5 \
	--enable-imbatchreport \
	--enable-imczmq \
	--enable-imdiag \
	--enable-imdocker \
	--enable-imfile \
	--enable-imhttp \
	--enable-imjournal \
	--enable-imkafka \
	--enable-impcap \
	--enable-improg \
	--enable-impstats \
	--enable-imptcp \
	--enable-imtuxedoulog \
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
	--enable-pmdb2diag \
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
	--enable-compile-warning=error \
	--enable-testbench

# module needs fixes: --enable-kmsg
# --enable-imdocker-tests is not supported as it needs to run on docker HOST
RUN	printf '\n' > /var/log/mysql/error.log
VOLUME	/var/lib/mysql
WORKDIR	/rsyslog
USER	rsyslog
