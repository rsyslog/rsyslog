# container for rsyslog development
# creates the build environment
# to search for packages:
# note: czmq OBS link: https://build.opensuse.org/package/show/network:messaging:zeromq:release-stable/czmq
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
	cmake \
	curl \
	flex \
	gcc \
	gdb \
	git \
	gzip \
	hiredis-devel \
	java-11-openjdk-devel \
	krb5-devel \
	libcurl-devel \
	libczmq4 \
	libdbi-devel \
	libdbi-drivers-dbd-mysql \
	libestr-devel \
	libfaketime \
	libgcrypt-devel \
	libgnutls-devel \
	libmaxminddb-devel \
	libmysqlclient-devel \
	libnet-devel \
	libopenssl-devel \
	libpcap-devel \
	libqpid-proton11 \
	libtool \
	libuuid-devel \
	libzstd-devel \
	make \
	net-snmp-devel \
	pcre-devel \
	postgresql-devel \
	python3-docutils \
	python-devel \
	python310-Sphinx \
	python310-pysnmp \
	qpid-proton \
	qpid-proton-devel \
	sudo \
	systemd-devel \
	tcl-devel \
	util-linux-systemd \
	valgrind \
	vi \
	wget \
	which \
	zlib-devel \
	zstd
RUN	zypper --non-interactive addrepo https://download.opensuse.org/repositories/network:messaging:zeromq:release-stable/openSUSE_Tumbleweed/network:messaging:zeromq:release-stable.repo && \
	zypper --non-interactive --gpg-auto-import-keys refresh && \
	zypper --non-interactive install \
	openpgm-devel \
	czmq \
	czmq-devel

ENV	REBUILD=1
WORKDIR /home/devel
RUN	groupadd rsyslog && \
	useradd -g rsyslog  -s /bin/bash rsyslog && \
	echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

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
RUN	mkdir /rsyslog && \
	chown rsyslog:rsyslog /rsyslog
VOLUME /rsyslog
ENV	PKG_CONFIG_PATH=/usr/lib/pkgconfig \
	xLD_LIBRARY_PATH=/usr/local/lib 

# create dependency cache
RUN	mkdir /local_dep_cache && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-6.0.0.tar.gz -O /local_dep_cache/elasticsearch-6.0.0.tar.gz
RUN	wget -nv https://dlcdn.apache.org/zookeeper/zookeeper-3.6.3/apache-zookeeper-3.6.3-bin.tar.gz -O /local_dep_cache/apache-zookeeper-3.6.2-bin.tar.gz
RUN	wget -nv https://www.apache.org/dyn/closer.cgi?path=/kafka/2.8.0/kafka_2.13-2.8.0.tgz -O /local_dep_cache/kafka_2.13-2.8.0.tgz
# tell tests which are the newester versions, so they can be checked without the need
# to adjust test sources.
#ENV	ELASTICSEARCH_NEWEST="elasticsearch-6.3.1.tar.gz"

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

# libmongoc is unfortunately not available on openSuse - later?
RUN	cd helper-projects && \
	wget -nv https://github.com/mongodb/mongo-c-driver/releases/download/1.12.0/mongo-c-driver-1.12.0.tar.gz && \
	tar xzf mongo-c-driver-1.12.0.tar.gz && \
	cd mongo-c-driver-1.12.0 && \
	mkdir cmake-build && \
	cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF && \
	make -j4 && \
	make install && \
	cd .. && \
	rm -r mongo-c-driver-1.12.0* && \
	cd ..

# bump dependency version below to trigger a dependency rebuild
# but not a full one (via --no-cache)
ENV	RSYSLOG_DEP_VERSION=2021-01-26

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
ENV	SCAN_BUILD=scan-build \
	SCAN_BUILD_CC=clang-6.0

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
	--enable-imbatchreport \
	--enable-imdiag \
	--enable-imdocker \
	--enable-imfile \
	--enable-imjournal \
	--enable-imkafka \
	--enable-impstats \
	--enable-impcap \
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
	--enable-mmcount \
	--enable-mmcapture \
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
	--enable-omhttp \
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
	--enable-snmp-tests \
	--enable-usertools \
	--enable-valgrind \
	--enable-gssapi-krb5 \
	--enable-omtcl \
	--enable-imczmq \
	--enable-omczmq \
	--enable-mmdblookup \
	--enable-kmsg \
	--enable-ommongodb \
	\
	--enable-testbench

# module needs fixes:
#	--enable-mmgrok
#	   -> we cannot build, libtokyocabinet dependency is not available as well
ENV	PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/local/lib64/pkgconfig/
WORKDIR /rsyslog
ARG	BUILD_TIME
ENV	CONTAINER_BUILD_TIME=$BUILD_TIME
USER rsyslog
