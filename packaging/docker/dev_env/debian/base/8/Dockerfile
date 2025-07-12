# container for rsyslog development
# creates the build environment
FROM	debian:8
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
	tcl-dev \
	uuid-dev \
	valgrind \
	vim \
	wget \
	sudo \
	logrotate \
	zlib1g-dev
#	libbson-dev
#	libmaxminddb-dev 
#	libmongoc-dev 
#RUN     add-apt-repository ppa:adiscon/v8-stable -y && \
#	add-apt-repository ppa:qpid/released -y && \
#	add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
#	echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-5.0 main" > /etc/apt/sources.list.d/llvm.list && \
#	wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key| apt-key add - && \
#	echo "deb http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_16.04/ ./" > /etc/apt/sources.list.d/0mq.list && \
#	wget -O - http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_16.04/Release.key | apt-key add -
# note: ppa:ubuntu-toolchain-r/test is currently the best repo for gcc-7 we can find...

RUN	apt-get update -y && \
	apt-get install -y \
	libestr-dev \
	libsodium-dev
#	clang-tools-5.0 
#	libfastjson-dev 
#	libczmq-dev 
#	clang-5.0 
#	gcc-7 
#	libqpid-proton10-dev 

WORKDIR	/home/devel
RUN	groupadd rsyslog \
	&& useradd -g rsyslog  -s /bin/bash rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
RUN	mkdir /rsyslog \
	&& chown rsyslog:rsyslog /rsyslog
VOLUME	/rsyslog
ENV	PKG_CONFIG_PATH=/usr/lib64/pkgconfig \
	xLD_LIBRARY_PATH=/usr/lib \
	DEBIAN_FRONTEND=

# create dependency cache
RUN	mkdir /local_dep_cache && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-5.6.9.tar.gz -O /local_dep_cache/elasticsearch-5.6.9.tar.gz  && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-6.0.0.tar.gz -O /local_dep_cache/elasticsearch-6.0.0.tar.gz && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-6.3.1.tar.gz -O /local_dep_cache/elasticsearch-6.3.1.tar.gz
# tell tests which are the newester versions, so they can be checked without the need
# to adjust test sources.
ENV	ELASTICSEARCH_NEWEST="elasticsearch-6.3.1.tar.gz"

# bump dependency version below to trigger a dependency rebuild
# but not a full one (via --no-cache)
ENV	DEP_VERSION=1
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

# we need Guardtime libksi here, otherwise we cannot check the KSI component	
RUN	cd helper-projects && \
	git clone https://github.com/guardtime/libksi.git && \
	cd libksi && \
	autoreconf -fvi && \
	./configure --libdir=/usr/lib64 && \
	make -j2 && \
	make install && \
	cd .. && \
	rm -r libksi && \
	cd ..

#RUN	apt-get install -y \
#	liblz4-dev

# we need the latest librdkafka as there as always required updates
#RUN	cd helper-projects && \
#	git clone https://github.com/edenhill/librdkafka && \
#	cd librdkafka && \
#	(unset CFLAGS; ./configure --prefix=/usr --libdir=/usr/lib64 --CFLAGS="-g" ; make -j) && \
#	make install && \
#	cd .. && \
# Note: we do NOT delete the source as we may need it to
# uninstall (in case the user wants to go back to system-default)
#	cd ..

# libmongoc is unfortunately not available on openSuse - later?
#RUN	cd helper-projects && \
#	wget -nv https://github.com/mongodb/mongo-c-driver/releases/download/1.12.0/mongo-c-driver-1.12.0.tar.gz && \
#	tar xzf mongo-c-driver-1.12.0.tar.gz && \
#	cd mongo-c-driver-1.12.0 && \
#	mkdir cmake-build && \
#	cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF && \
#	make -j4 && \
#	make install && \
#	cd .. && \
#	rm -r mongo-c-driver-1.12.0* && \
#	cd ..

# bump dependency version below to trigger a dependency rebuild
# but not a full one (via --no-cache)
ENV	RSYSLOG_DEP_VERSION=2018-09-16

# libestr - currently, not needed, we use from offical repo (unlikely to change)
#RUN	cd helper-projects && \
#	git clone https://github.com/rsyslog/libestr.git && \
#	cd libestr && \
#	autoreconf -fi && ./configure --libdir=/usr/lib64 --prefix=/usr && \
#	make -j4 && \
#	make install && \
#	cd .. && \
#	rm -r libestr && \
#	cd ..

# liblogging
RUN	cd helper-projects && \
	git clone https://github.com/rsyslog/liblogging.git && \
	cd liblogging && \
	autoreconf -fi && \
	./configure --prefix=/usr --libdir=/usr/lib64 --disable-journal && \
	make -j4 && \
	make install && \
	cd .. && \
	rm -r liblogging && \
	cd ..

# liblfastjson
RUN	cd helper-projects && \
	git clone https://github.com/rsyslog/libfastjson.git && \
	cd libfastjson && \
	autoreconf -fi && \
	./configure --prefix=/usr --libdir=/usr/lib64 && \
	make -j4 && \
	make install && \
	cd .. && \
	rm -r libfastjson && \
	cd ..

# liblognorm
RUN	cd helper-projects && \
	git clone https://github.com/rsyslog/liblognorm.git && \
	cd liblognorm && \
	autoreconf -fi && \
	./configure --prefix=/usr --libdir=/usr/lib64 && \
	make -j4 && \
	make install && \
	cd .. && \
	rm -r liblognorm && \
	cd ..

# librelp
RUN	cd helper-projects && \
	git clone https://github.com/rsyslog/librelp.git && \
	cd librelp && \
	autoreconf -fi && \
	./configure --prefix=/usr --enable-compile-warnings=yes --libdir=/usr/lib64 && \
	make -j4 && \
	make install && \
	cd .. && \
	rm -r librelp && \
	cd ..

# next ENV is specifically for running scan-build - so we do not need to
# change scripts if at a later time we can move on to a newer version
ENV SCAN_BUILD=scan-build \
    SCAN_BUILD_CC=clang-5.0

ENV RSYSLOG_CONFIGURE_OPTIONS \
	--enable-elasticsearch \
	--enable-elasticsearch-tests \
	--enable-gnutls \
	--enable-gssapi-krb5 \
	--enable-imbatchreport \
	--disable-imczmq \
	--enable-imdiag \
	--enable-imfile \
	--enable-imjournal \
	--disable-imkafka \
	--enable-impstats \
	--enable-improg \
	--enable-imptcp \
	--enable-impcap \
	--enable-imtuxedolog \
	--disable-kafka-tests \
	--disable-kmsg \
	--enable-ksi-ls12 \
	--enable-libdbi \
	--enable-libfaketime \
	--enable-libgcrypt \
	--enable-mail \
	--enable-mmanon \
	--enable-mmaudit \
	--enable-mmcount \
	--disable-mmdblookup \
	--enable-mmfields \
	--enable-mmgrok \
	--enable-mmjsonparse \
	--enable-mmkubernetes \
	--enable-mmnormalize \
	--enable-mmpstrucdata \
	--enable-mmrm1stspace \
	--enable-mmsequence \
	--enable-mmsnmptrapd \
	--enable-mmtaghostname \
	--enable-mmutf8fix \
	--enable-mysql \
	--disable-omamqp1 \
	--disable-omczmq \
	--enable-omhttpfs \
	--enable-omjournal \
	--disable-omkafka \
	--disable-ommongodb \
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
	--enable-pmdb2diag \
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
