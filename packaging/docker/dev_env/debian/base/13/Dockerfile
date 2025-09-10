# container for rsyslog development
# creates the build environment
FROM	debian:13
ENV	DEBIAN_FRONTEND=noninteractive
RUN 	apt-get update && \
	apt-get upgrade -y \
	&& apt-get install -y \
	autoconf \
	autoconf-archive \
	automake \
	autotools-dev \
	bison \
	clang \
	clang-17 \
	clang-tools-17 \
	curl \
	default-jdk \
	faketime \
	flex \
	gdb \
	git \
	libaprutil1-dev \
	libcivetweb-dev \
	libcurl4-gnutls-dev \
	libdbd-mysql \
	libdbi-dev \
	libgcrypt20-dev \
	libglib2.0-dev \
	libgnutls28-dev \
	libhiredis-dev \
	libkrb5-dev \
	liblz4-dev \
	libmaxminddb-dev \
	default-libmysqlclient-dev \
	libnet1-dev \
	libpcap-dev \
	libpq-dev \
	libsasl2-dev \
	libsnmp-dev \
	libssl-dev \
	libsystemd-dev \
	libtokyocabinet-dev \
	libtool \
	libzstd-dev \
	default-mysql-server \
	net-tools \
	pkg-config \
	postgresql-client \
	python3-docutils \
	python3-pysnmp4 \
	python3-pysnmp4-apps \
	python3-pysnmp4-mibs \
	tcl-dev \
	uuid-dev \
	valgrind \
	vim \
	wget \
	zlib1g-dev \
	libmongoc-dev \
	zstd \
	&& apt clean
#	libbson-dev
#	libgrok1 libgrok-dev
#	software-properties-common
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
	libfastjson-dev \
	libsodium-dev

# Create libgcrypt-config wrapper for Debian 13 compatibility
# (libgcrypt-config is not provided in Debian 13, but pkg-config is available)
RUN	printf '#!/bin/sh\ncase "$1" in\n  --version)\n    echo "%s"\n    ;;\n  --cflags)\n    pkg-config --cflags libgcrypt\n    ;;\n  --libs)\n    pkg-config --libs libgcrypt\n    ;;\n  *)\n    echo "Usage: $0 {--version|--cflags|--libs}"\n    exit 1\n    ;;\nesac\n' "$(pkg-config --modversion libgcrypt)" > /usr/bin/libgcrypt-config && chmod +x /usr/bin/libgcrypt-config
#	clang-tools-17
#	libfastjson-dev 
#	libczmq-dev 
#	clang-17 
#	gcc-7 
#	libqpid-proton10-dev 

WORKDIR	/home/devel
RUN	groupadd rsyslog \
	&& useradd -g rsyslog  -s /bin/bash rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
# we need this for sudo to work...
RUN	mkdir /rsyslog \
	&& chown rsyslog:rsyslog /rsyslog
VOLUME	/rsyslog
ENV	PKG_CONFIG_PATH=/usr/lib64/pkgconfig \
	LD_LIBRARY_PATH=/usr/lib \
	DEBIAN_FRONTEND=

# bump dependency version below to trigger a dependency rebuild
# but not a full one (via --no-cache)
ENV	DEP_VERSION=1
# Helper projects and dependency build starts here
RUN	mkdir helper-projects

# we need Guardtime libksi here, otherwise we cannot check the KSI component	
RUN	cd helper-projects && \
	git clone https://github.com/guardtime/libksi.git && \
	cd libksi && \
	autoreconf -fvi && \
	./configure --libdir=/usr/lib64 && \
	make -j && \
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

# bump dependency version below to trigger a dependency rebuild
# but not a full one (via --no-cache)
ENV	RSYSLOG_DEP_VERSION=2020-01-26

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
	make -j && \
	make install && \
	cd .. && \
	rm -r liblogging && \
	cd ..

# libfastjson - using system package instead of building from source
# (system package is available in Debian 13)

# liblognorm
RUN	cd helper-projects && \
	git clone https://github.com/rsyslog/liblognorm.git && \
	cd liblognorm && \
	autoreconf -fi && \
	./configure --prefix=/usr --libdir=/usr/lib64 --disable-Werror && \
	make -j && \
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
	make -j && \
	make install && \
	cd .. && \
	rm -r librelp && \
	cd ..

# next ENV is specifically for running scan-build - so we do not need to
# change scripts if at a later time we can move on to a newer version
ENV	SCAN_BUILD=scan-build \
	SCAN_BUILD_CC=clang-17

ENV	RSYSLOG_CONFIGURE_OPTIONS \
	--enable-elasticsearch \
	--enable-elasticsearch-tests \
	--enable-gnutls \
	--enable-gssapi-krb5 \
	--enable-imbatchreport \
	--disable-imczmq \
	--enable-imdiag \
	--enable-imdocker \
	--enable-imfile \
	--enable-imhttp \
	--enable-imjournal \
	--disable-imkafka \
	--enable-impstats \
	--enable-improg \
	--enable-imptcp \
	--enable-impcap \
	--enable-imtuxedolog \
	--disable-kafka-tests \
	--enable-kmsg \
	--enable-ksi-ls12 \
	--enable-libdbi \
	--enable-libfaketime \
	--enable-libgcrypt \
	--enable-libzstd \
	--enable-mail \
	--enable-mmanon \
	--enable-mmaudit \
	--enable-mmcount \
	--enable-mmdblookup \
	--enable-mmfields \
	--disable-mmgrok \
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
	--enable-omhiredis \
	--enable-omhttp \
	--enable-omhttpfs \
	--enable-omjournal \
	--disable-omkafka \
	--enable-ommongodb \
	--enable-omclickhouse \
	--enable-omprog \
	--enable-omrelp-default-port=13515 \
	--enable-omruleset \
	--enable-omsendertrack \
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
	# SNMP Tests disabled due to Python 3.12+ compatibility issues
	# TODO: Re-enable once pysnmp is updated to support Python 3.12+
	# See: https://github.com/rsyslog/rsyslog/issues/5999
	#--enable-snmp-tests \
	--enable-usertools \
	--enable-valgrind \
	\
	--enable-testbench

RUN	echo merge me up \
	&& groupadd -g 1001 rsyslog999 \
	&& groupadd -g 1002 rsyslog998 \
	&& groupadd -g 1003 rsyslog997 \
	&& groupadd -g 1004 rsyslog996 \
	&& groupadd -g 1005 rsyslog995 \
	&& echo missing \
	&& useradd -u 1001 -g rsyslog999  -s /bin/bash rsyslog999 \
	&& useradd -u 1002 -g rsyslog998  -s /bin/bash rsyslog998 \
	&& useradd -u 1003 -g rsyslog997  -s /bin/bash rsyslog997 \
	&& useradd -u 1004 -g rsyslog996  -s /bin/bash rsyslog996 \
	&& useradd -u 1005 -g rsyslog995  -s /bin/bash rsyslog995 \
	&& echo "rsyslog999 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers \
	&& echo "rsyslog998 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers \
	&& echo "rsyslog997 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers \
	&& echo "rsyslog996 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers \
	&& echo "rsyslog995 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
WORKDIR /rsyslog
USER rsyslog
