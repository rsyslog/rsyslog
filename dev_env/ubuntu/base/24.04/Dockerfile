# container for rsyslog development
# creates the build environment
FROM	ubuntu:24.04
ENV	DEBIAN_FRONTEND="noninteractive"
RUN 	apt-get update && \
	apt-get upgrade -y
RUN	apt-get install -y \
	autoconf \
	autoconf-archive
RUN	apt-get install -y \
	automake \
	autotools-dev \
	bison \
	clang \
	gdb \
	clang-tools \
	curl 
RUN	apt-get install -y \
	default-jdk \
	default-jre \
	faketime libdbd-mysql \
	flex \
	gcc \
	gcc-14 \
	gdb \
	git \
	libaprutil1-dev \
	libbson-dev \
	libcap-ng-dev \
	libcap-ng0 \
	libcurl4-gnutls-dev \
	libdbi-dev \
	libgcrypt20-dev \
	libglib2.0-dev \
	libgnutls28-dev \
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
	libtirpc-dev \
	libtokyocabinet-dev \
	libtool \
	libtool-bin \
	libzstd-dev \
	logrotate \
	lsof \
	make \
	mysql-server \
	net-tools \
	pkg-config \
	postgresql-client libpq-dev \
	python3-docutils  \
	python3-pip \
	python3-pysnmp4 \
	software-properties-common \
	sudo \
	uuid-dev \
	valgrind \
	vim \
	wget \
	zlib1g-dev \
	zstd
#	libgrok1 libgrok-dev \
ENV	REBUILD=1
# Adiscon/rsyslog components
#RUN	apt-key adv --keyserver keyserver.ubuntu.com --recv E0C56BD4 && \
 	#add-apt-repository ppa:adiscon/v8-stable -y && \
	# END
RUN	echo 'deb http://download.opensuse.org/repositories/home:/rgerhards/xUbuntu_22.04/ /' > /etc/apt/sources.list.d/home:rgerhards.list && \
	wget -nv https://download.opensuse.org/repositories/home:rgerhards/xUbuntu_22.04/Release.key -O Release.key && \
	apt-key add - < Release.key && \
	apt-get update -y && \
	apt-get install -y  \
	libestr-dev \
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

# create dependency cache
RUN	mkdir /local_dep_cache
RUN	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-6.8.23.tar.gz -O /local_dep_cache/elasticsearch-6.8.23.tar.gz
RUN	wget -nv https://www.rsyslog.com/files/download/rsyslog/elasticsearch-7.14.1-linux-x86_64.tar.gz -O /local_dep_cache/elasticsearch-7.14.1-linux-x86_64.tar.gz
RUN	wget -nv https://dlcdn.apache.org/zookeeper/zookeeper-3.9.3/apache-zookeeper-3.9.3-bin.tar.gz -O /local_dep_cache/apache-zookeeper-3.9.3-bin.tar.gz
RUN	wget -nv https://www.apache.org/dyn/closer.cgi?path=/kafka/2.8.0/kafka_2.13-2.8.0.tgz -O /local_dep_cache/kafka_2.13-2.8.0.tgz
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
RUN	mkdir /rsyslog
VOLUME	/rsyslog
RUN	groupadd rsyslog \
	&& useradd -g rsyslog  -s /bin/bash rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers \
	&& echo "buildbot ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# mysql needs a little help:
RUN	mkdir -p /var/run/mysqld && \
	chown mysql:mysql /var/run/mysqld
ENV	MYSQLD_START_CMD="sudo mysqld_safe --pid-file=/var/run/mysqld/mysqld.pid" \
        MYSQLD_STOP_CMD="sudo kill $(sudo cat /var/run/mysqld/mysqld.pid)"

# and so does clickhouse
#RUN	chown root:root /var/lib/clickhouse
#ENV	CLICKHOUSE_START_CMD="sudo clickhouse-server --config-file=/etc/clickhouse-server/config.xml" \
#	CLICKHOUSE_STOP_CMD="sudo kill $(pidof clickhouse-server)"

ENV	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
	LD_LIBRARY_PATH=/usr/local/lib \
	DEBIAN_FRONTEND= \
	SUDO="sudo -S"

# bump dependency version below to trigger a dependency rebuild
# but not a full one (via --no-cache)
ENV	DEP_VERSION=4
# Helper projects and dependency build starts here
RUN	mkdir helper-projects
# code style checker - not yet packaged
RUN	cd helper-projects && \
	git clone https://github.com/rsyslog/codestyle && \
	cd codestyle && \
	gcc --std=c99 stylecheck.c -o stylecheck && \
	mv stylecheck /usr/bin/rsyslog_stylecheck && \
	cd .. && \
	rm -r codestyle && \
	cd ..


# enable manual build when needed
# libestr
#RUN	cd helper-projects && \
#	git clone https://github.com/rsyslog/libestr.git && \
#	cd libestr && \
#	autoreconf -fi && \
#	./configure --prefix=/usr/local && \
#	make -j4 && \
#	make install && \
#	cd .. && \
#	rm -r libestr && \
#	cd ..

# liblogging
#RUN	cd helper-projects && \
#	git clone https://github.com/rsyslog/liblogging.git && \
#	cd liblogging && \
#	autoreconf -fi && \
#	./configure --prefix=/usr --disable-journal && \
#	make -j4 && \
#	make install && \
#	cd .. && \
#	rm -r liblogging && \
#	cd ..

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

# liblognorm
#RUN	cd helper-projects && \
#	git clone https://github.com/rsyslog/liblognorm.git && \
#	cd liblognorm && \
#	autoreconf -fi && \
#	./configure --enable-compile-warnings=yes --prefix=/usr/local && \
#	make -j4 && \
#	make install && \
#	cd .. && \
#	rm -r liblognorm && \
#	cd ..

# we need libfaup for some modules - packages do usually not exist
RUN	apt-get -y install cmake
RUN	cd helper-projects && \
	git clone https://github.com/stricaud/faup.git && \
	cd faup && \
	cd build && \
	cmake .. && make -j&& \
	make install && \
	cd .. && \
	cd .. && \
	rm -r faup && \
	cd ..

# we need Guardtime libksi here, otherwise we cannot check the KSI component	
RUN	cd helper-projects && \
	git clone https://github.com/guardtime/libksi.git && \
	cd libksi && \
	autoreconf -fvi && \
	./configure --prefix=/usr && \
	make -j && \
	make install && \
	cd .. && \
	rm -r libksi && \
	cd ..

# librelp
ENV	DEP_VERSION=5
RUN	cd helper-projects && \
	git clone https://github.com/rsyslog/librelp.git && \
	cd librelp && \
	autoreconf -fi && \
	./configure --prefix=/usr --enable-compile-warnings=yes --libdir=/usr/lib --includedir=/usr/include && \
	make -j && \
	make install && \
	cd .. && \
	rm -r librelp && \
	cd ..

# we need the latest librdkafka as there as always required updates -- not necessarily any longer!
RUN	cd helper-projects && \
	git clone https://github.com/edenhill/librdkafka && \
	cd librdkafka && \
	git checkout v1.5.3 && \
	(unset CFLAGS; ./configure --prefix=/usr --libdir=/usr/lib --CFLAGS="-g" ; make -j) && \
	make install && \
	cd .. && \
	cd ..

# kafkacat
RUN	cd helper-projects \
	&& git clone https://github.com/edenhill/kafkacat \
	&& cd kafkacat \
	&& (unset CFLAGS; ./configure --prefix=/usr --CFLAGS="-g" ; make -j) \
	&& make install \
	&& cd .. \
	&& cd ..

# we need civetweb, as there are no packages for it
RUN	cd helper-projects && \
	git clone https://github.com/civetweb/civetweb.git \
	&& cd civetweb \
	&& (unset CFLAGS; make -j build COPT="-DREENTRANT_TIME"; make install-headers ; make install-slib ) \
	&& cd .. \
	&& rm -rf civetweb \
	&& cd ..

# next ENV is specifically for running scan-build - so we do not need to
# change scripts if at a later time we can move on to a newer version
ENV	SCAN_BUILD=scan-build-9 \
	SCAN_BUILD_CC=clang-9 \
	ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-9/bin/llvm-symbolizer

RUN	chmod o+rx /var/run/mysqld && ls -ld /var/run/mysqld

ENV RSYSLOG_CONFIGURE_OPTIONS=" \
	--enable-compile-warning=error \
	--enable-clickhouse \
	--enable-clickhouse-tests=no \
	--enable-elasticsearch \
	--enable-elasticsearch-tests \
	--enable-ffaup \
	--enable-gnutls \
	--enable-gssapi-krb5 \
	--enable-imbatchreport \
	--enable-imczmq \
	--enable-imdiag \
	--enable-imdocker \
	--disable-imdocker-tests \
	--enable-imfile \
	--enable-imhttp \
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
	--enable-libzstd \
	--enable-mail \
	--enable-mmanon \
	--enable-mmaudit \
	--enable-mmcapture \
	--enable-mmcount \
	--enable-mmdarwin \
	--enable-mmdblookup \
	--enable-mmfields \
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
	--enable-imdtls \
	--enable-omdtls \
	--enable-valgrind \
	\
	--enable-testbench \
	"
# libgrok seems no longer to be available
# --enable-mmgrok \


# Note: imdocker-tests cannot be run inside container, as they need to talk
# to a docker instance.
# module needs fixes: --enable-kmsg
RUN	printf '\n' > /var/log/mysql/error.log
# fix up llvm symbolizer - not correct for some reason
#RUN	ln -s /usr/bin/asan_symbolize /usr/bin/llvm-symb
#ENV	ASAN_SYMBOLIZER_PATH="/usr/bin/llvm-symbolizer"
ENV	ASAN_SYMBOLIZER_PATH="/usr/lib/llvm-18/bin/llvm-symbolizer"
VOLUME	/var/lib/mysql
ENV	C_INCLUDE_PATH="/usr/include/tirpc/"
WORKDIR	/rsyslog
USER	rsyslog
