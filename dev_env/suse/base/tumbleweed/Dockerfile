# container for rsyslog development
# creates the build environment
# to search for packages:
# 1. zypper search
# 2. scout bin which  # scout need zypper install scout - then call "scout" for usage
FROM	opensuse/tumbleweed
RUN 	zypper --non-interactive update
RUN	zypper --non-interactive install \
	autoconf \
	autoconf-archive \
	automake \
	bison \
	clang \
	curl \
	flex \
	gcc \
	gdb \
	git \
	hiredis-devel \
	java-10-openjdk-devel \
	krb5-devel \
	libcurl-devel \
	libczmq4 \
	libdbi-devel \
	libdbi-drivers-dbd-mysql \
	libfaketime \
	libgcrypt-devel \
	libgnutls-devel \
	libmysqlclient-devel \
	libnet-devel \
	libopenssl-devel \
	libqpid-proton10 \
	libtool \
	libuuid-devel \
	make \
	net-snmp-devel \
	pcre-devel \
	postgresql-devel \
	python3-docutils \
	python-devel \
	python-docutils \
	python-sphinx \
	qpid-proton \
	qpid-proton-devel \
	systemd-devel \
	tcl-devel \
	valgrind \
	vi \
	wget \
	zlib-devel
ENV	REBUILD=1
#RUN	echo "deb http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_18.04/ ./" > /etc/apt/sources.list.d/0mq.list && \
	#wget -nv -O - http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_18.04/Release.key | apt-key add - && \
 	#add-apt-repository ppa:adiscon/v8-stable -y && \
	#apt-get update -y && \
	#apt-get install -y  \
	#libczmq-dev \
	#libqpid-proton8-dev \
	#tcl-dev \
	#libsodium-dev \
	#libestr-dev \
	#librelp-dev \
	#libfastjson-dev \
	#liblogging-stdlog-dev \
	#liblognorm-dev
WORKDIR /home/devel
RUN	groupadd rsyslog && \
	useradd -g rsyslog  -s /bin/bash rsyslog && \
	echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
RUN	mkdir /rsyslog && \
	chown rsyslog:rsyslog /rsyslog
VOLUME /rsyslog
ADD	setup-projects.sh setup-projects.sh
ENV	PKG_CONFIG_PATH=/usr/lib/pkgconfig \
	xLD_LIBRARY_PATH=/usr/local/lib 

# Install any needed packages
#RUN	./setup-system.sh

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
# we need the latest librdkafka as there as always required updates
RUN	cd helper-projects && \
	git clone https://github.com/edenhill/librdkafka && \
	cd librdkafka && \
	(unset CFLAGS; ./configure --prefix=/usr --libdir=/usr/lib64 --CFLAGS="-g" ; make -j) && \
	make install && \
	cd .. && \
# Note: we do NOT delete the source as we may need it to
# uninstall (in case the user wants to go back to system-default)
	cd ..

# bump dependency version below to trigger a dependency rebuild
# but not a full one (via --no-cache)
ENV	RSYSLOG_DEP_VERSION=2018-07-22

# libestr
RUN	cd helper-projects && \
	git clone https://github.com/rsyslog/libestr.git && \
	cd libestr && \
	autoreconf -fi && ./configure --libdir=/usr/lib64 --prefix=/usr && \
	make -j4 && \
	make install && \
	cd .. && \
	rm -r libestr && \
	cd ..

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
ENV	SCAN_BUILD=scan-build \
	SCAN_BUILD_CC=clang-6.0

RUN	zypper --non-interactive install \
	util-linux-systemd \
	which
# unfortunately, tcl-devel does not properly setup required bits in the environment
# so we now try to do that. In case this does no longer work with a version, search
# for a file tclConfig.sh, which should be present in the library directory (usually
# beneath /usr). It contains the environment variables. Inside container do:
# $cat $(find /usr -name tclConfig.sh|head -n1)
ENV	TCL_LIB_SPEC="-L/usr/lib64 -ltcl8.6" \
	TCL_INCLUDE_SPEC=-I/usr/include

ENV RSYSLOG_CONFIGURE_OPTIONS \
	--enable-elasticsearch \
	--enable-elasticsearch-tests \
	--enable-gnutls \
	--enable-imdiag \
	--enable-imfile \
	--enable-imjournal \
	--enable-imkafka \
	--enable-impstats \
	--enable-imptcp \
	--enable-kafka-tests \
	--enable-ksi-ls12 \
	--enable-libdbi \
	--enable-libfaketime \
	--enable-libgcrypt \
	--enable-mail \
	--enable-mmanon \
	--enable-mmaudit \
	--enable-mmcount \
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
	--enable-omamqp1 \
	--enable-omhiredis \
	--enable-omhiredis \
	--enable-omhttpfs \
	--enable-omjournal \
	--enable-omkafka \
	--enable-omprog \
	--enable-omrelp-default-port=13515 \
	--enable-omruleset \
	--enable-omstdout \
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
	--enable-gssapi-krb5 \
	--enable-omtcl \
	\
	--enable-testbench

# module needs fixes:
#	--enable-kmsg
#	--enable-ommongodb
#	--enable-imczmq
#	--enable-omczmq
#	--enable-mmdblookup \
#	--enable-mmgrok \
WORKDIR /rsyslog
USER rsyslog
