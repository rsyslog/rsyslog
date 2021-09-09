# container for rsyslog development
# creates the build environment
# Note: this image currently uses in-container git checkouts to
# build the "rsyslog libraries" - we do not have packages for them
FROM	centos:7
# search for packages that contain <file>: yum whatprovides <file>
RUN	yum -y update && \
	yum -y install \
	apr-util-devel \
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
	libnet libnet-devel \
	libpcap-devel \
	librabbitmq-devel \
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
	python-pysnmp \
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
	hiredis \
	hiredis-devel \
	libmaxminddb \
	libmaxminddb-devel \
	mongo-c-driver \
	mongo-c-driver-devel
	# end of this RUN
RUN	wget -O /etc/yum.repos.d/network:messaging:zeromq:git-stable.repo https://download.opensuse.org/repositories/network:messaging:zeromq:git-stable/CentOS_7/network:messaging:zeromq:git-stable.repo && \
	yum -y install czmq-devel
	# end of this RUN
	#wget -O /etc/yum.repos.de/network:messaging:zeromq:git-stable.repo  https://download.opensuse.org/repositories/network:messaging:zeromq:git-stable/CentOS_6/network:messaging:zeromq:git-stable.repo && \
RUN 	find / -name "pgm*"
RUN	mkdir /usr/lib64/pgm-5.2
RUN	mkdir /usr/lib64/pgm-5.2/include
# unfortunately, tcl-devel does not properly setup required bits in the environment
# so we now try to do that. In case this does no longer work with a version, search
# for a file tclConfig.sh, which should be present in the library directory (usually
# beneath /usr). It contains the environment variables. Inside container do:
# $cat $(find /usr -name tclConfig.sh|head -n1)
ENV	TCL_LIB_SPEC="-L/usr/lib64 -ltcl8.5" \
	TCL_INCLUDE_SPEC=-I/usr/include

# create dependency cache
RUN	mkdir /local_dep_cache && \
	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-5.6.9.tar.gz -O /local_dep_cache/elasticsearch-5.6.9.tar.gz
RUN	wget -nv https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-6.0.0.tar.gz -O /local_dep_cache/elasticsearch-6.0.0.tar.gz
RUN	wget -nv https://dlcdn.apache.org/zookeeper/zookeeper-3.6.3/apache-zookeeper-3.6.3-bin.tar.gz -O /local_dep_cache/apache-zookeeper-3.6.2-bin.tar.gz
RUN	wget -nv https://www.apache.org/dyn/closer.cgi?path=/kafka/2.8.0/kafka_2.13-2.8.0.tgz -O /local_dep_cache/kafka_2.12-2.7.0.tgz
# tell tests which are the newester versions, so they can be checked without the need
# to adjust test sources.
#ENV	ELASTICSEARCH_NEWEST="elasticsearch-6.3.1.tar.gz"

WORKDIR /home/devel
RUN	groupadd rsyslog \
	&& adduser -g rsyslog  -s /bin/bash rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
RUN	mkdir /rsyslog \
	&& chown rsyslog:rsyslog /rsyslog
VOLUME /rsyslog

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
ENV	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
	LD_LIBRARY_PATH=/usr/local/lib \
	LIBDIR_PATH=/usr/lib64

# bump dependency version below to trigger a dependency rebuild
# but not a full one (via --no-cache)
ENV	DEP_VERSION=5
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

# libestr
RUN	cd helper-projects && \
	git clone https://github.com/rsyslog/libestr.git && \
	cd libestr && \
	autoreconf -fi && \
	./configure --prefix=/usr/local && \
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
	./configure --enable-compile-warnings=yes --prefix=/usr/local && \
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

# other manual installs
# kafkacat
RUN	cd helper-projects \
	&& git clone https://github.com/edenhill/kafkacat \
	&& cd kafkacat \
	&& (unset CFLAGS; ./configure --prefix=/usr --CFLAGS="-g" ; make -j2) \
	&& make install \
	&& cd .. \
	&& rm -r kafkacat \
	&& cd ..
# Note: we do NOT delete the source as we may need it to
# uninstall (in case the user wants to go back to system-default)

# we need civetweb, as there are no packages for it
RUN	cd helper-projects && \
	git clone https://github.com/civetweb/civetweb.git \
	&& cd civetweb \
	&& (unset CFLAGS; make -j build ; make install-headers ; make install-slib ) \
	&& cd .. \
	&& rm -rf civetweb \
	&& cd ..


# next ENV is specifically for running scan-build - so we do not need to
# change scripts if at a later time we can move on to a newer version
#ENV SCAN_BUILD=scan-build \
#    SCAN_BUILD_CC=clang-5.0

ENV RSYSLOG_CONFIGURE_OPTIONS \
	--enable-elasticsearch \
	--enable-elasticsearch-tests \
	--enable-gnutls \
	--enable-gssapi-krb5 \
	--enable-imbatchreport \
	--enable-imczmq \
	--enable-imdiag \
	--enable-imfile \
	--enable-imhttp \
	--enable-imjournal \
	--enable-imkafka \
	--enable-impstats \
	--enable-improg \
	--enable-imptcp \
	--enable-impcap \
	--enable-imtuxedolog \
	--enable-kafka-tests \
	--enable-ksi-ls12 \
	--enable-libdbi \
	--enable-libfaketime \
	--enable-libgcrypt \
	--enable-mail \
	--enable-mmanon \
	--enable-mmaudit \
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
	--enable-mmtaghostname \
	--enable-mmutf8fix \
	--enable-mysql \
	--enable-omamqp1 \
	--enable-omczmq \
	--enable-omhiredis \
	--enable-omhiredis \
	--enable-omhttpfs \
	--enable-omjournal \
	--enable-omkafka \
	--enable-ommongodb \
	--enable-omprog \
	--enable-omrabbitmq \
	--enable-omrelp-default-port=13515 \
	--enable-omruleset \
	--enable-omstdout \
	--enable-omtcl=no \
	--enable-omudpspoof \
	--enable-omuxsock \
	--enable-openssl \
	--enable-pgsql \
	--enable-pmciscoios \
	--enable-pmdb2diag \
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

# build errors at the moment: --enable-kmsg 
#	--enable-mmgrok - no package
WORKDIR /rsyslog
USER	rsyslog
