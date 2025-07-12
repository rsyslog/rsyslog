# container for rsyslog development
# creates the build environment
# Note: this image currently uses in-container git checkouts to
# build the "rsyslog libraries" - we do not have packages for them
FROM	centos:6
# search for packages that contain <file>: yum whatprovides <file>
RUN	yum -y update && \
	yum -y install \
	autoconf \
	autoconf-archive \
	automake \
	bison \
	clang \
	clang-analyzer \
	libstdc++ \
	compat-libstdc++-33 \
	curl \
	cyrus-sasl-devel \
	cyrus-sasl-lib \
	flex \
	gcc \
	gcc-c++ \
	gdb \
	git \
	gnutls-devel \
	hiredis-devel \
	java-1.8.0-openjdk \
	java-1.8.0-openjdk-devel \
	libcurl-devel \
	libdbi-dbd-mysql \
	libdbi-devel \
	libfaketime \
	libgcrypt-devel \
	libmaxminddb-devel \
	libstdc++ \
	libtool \
	libuuid-devel \
	lsof \
	mysql-devel \
	nc \
	net-snmp-devel \
	net-tools \
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
	tcl-devel \
	valgrind \
	wget \
	yum -y install \
	zlib-devel
	# end of this RUN
RUN	yum -y install epel-release && \
	yum -y install \
	czmq-devel \
	hiredis \
	hiredis-devel \
	libmaxminddb \
	libmaxminddb-devel \
	libnet libnet-devel \
	mongo-c-driver \
	mongo-c-driver-devel
	# end of this RUN
# unfortunately, tcl-devel does not properly setup required bits in the environment
# so we now try to do that. In case this does no longer work with a version, search
# for a file tclConfig.sh, which should be present in the library directory (usually
# beneath /usr). It contains the environment variables. Inside container do:
# $cat $(find /usr -name tclConfig.sh|head -n1)
ENV	TCL_LIB_SPEC="-L/usr/lib64 -ltcl8.5" \
	TCL_INCLUDE_SPEC=-I/usr/include

# create dependency cache
RUN	mkdir /local_dep_cache && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-5.6.9.tar.gz -O /local_dep_cache/elasticsearch-5.6.9.tar.gz  && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-6.0.0.tar.gz -O /local_dep_cache/elasticsearch-6.0.0.tar.gz && \
	wget -nv https://www-us.apache.org/dist/zookeeper/zookeeper-3.4.14/zookeeper-3.4.14.tar.gz -O /local_dep_cache/zookeeper-3.4.14.tar.gz
#	&& wget -nv http://www-us.apache.org/dist/kafka/2.5.0/kafka_2.12-2.5.0.tgz -O /local_dep_cache/kafka_2.12-2.5.0.tgz
# tell tests which are the newester versions, so they can be checked without the need
# to adjust test sources.
#ENV	ELASTICSEARCH_NEWEST="elasticsearch-6.3.1.tar.gz"

WORKDIR	/home/devel
VOLUME	/rsyslog
RUN	groupadd rsyslog \
	&& adduser -g rsyslog  -s /bin/bash rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
# we need this for sudo to work...
RUN	echo 'user1000:x:1000:1000::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1001:x:1001:1001::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1002:x:1002:1002::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1003:x:1003:1003::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1004:x:1004:1004::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1005:x:1005:1005::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1006:x:1006:1006::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1007:x:1007:1007::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1008:x:1008:1008::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1009:x:1009:1009::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'user1010:x:1010:1010::/home/rsyslog:/bin/bash' >> /etc/passwd && \
	echo 'grp1000:x:1000:user1000' >> /etc/group && \
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
	echo "user1000 ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
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
ADD	setup-projects.sh setup-projects.sh
ENV	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
	LD_LIBRARY_PATH=/usr/local/lib \
	LIBDIR_PATH=/usr/lib64

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
#RUN	cd helper-projects && \
#	git clone https://github.com/edenhill/librdkafka && \
##	cd librdkafka && \
#	(unset CFLAGS; ./configure --prefix=/usr --libdir=/usr/lib64 --CFLAGS="-g" ; make -j) && \
#	make install && \
#	cd .. && \
# Note: we do NOT delete the source as we may need it to
# uninstall (in case the user wants to go back to system-default)
RUN	cd ..

# next ENV is specifically for running scan-build - so we do not need to
# change scripts if at a later time we can move on to a newer version
#ENV SCAN_BUILD=scan-build \
#    SCAN_BUILD_CC=clang-5.0

ENV RSYSLOG_CONFIGURE_OPTIONS \
	--enable-elasticsearch \
	--enable-elasticsearch-tests \
	--enable-gnutls \
	--enable-gssapi-krb5 \
	--enable-imdiag \
	--enable-imfile \
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
	--enable-omudpspoof \
	--enable-omhttpfs \
	--enable-omprog \
	--enable-omrelp-default-port=13515 \
	--enable-omruleset \
	--enable-omstdout \
	--enable-omtcl=no \
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

# build errors at the moment: --enable-kmsg 
#	--enable-imjournal  not available on this platform
#	--enable-omjournal  not available on this platform
#	--enable-ommongodb  not available on this platform
#	--enable-omczmq
#	--enable-imczmq 
#	--enable-omamqp1  removed as I *think* it is not supported here
#	--enable-omhiredis 
#	--enable-mmgrok - no package
#	--enable-imkafka - librdkafka no longer builds
#	--enable-omkafka - librdkafka no longer builds

# Install any needed packages
RUN ./setup-projects.sh
WORKDIR /rsyslog
RUN	chown rsyslog:rsyslog /rsyslog
USER rsyslog
