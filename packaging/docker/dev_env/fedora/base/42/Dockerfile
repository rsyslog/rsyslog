# container for rsyslog development
# creates the build environment
# Note: this image currently uses in-container git checkouts to
# build the "rsyslog libraries" - we do not have packages for them
FROM	fedora:42
# search for packages that contain <file>: dnf whatprovides <file>
RUN	dnf -y update
RUN	dnf -y install \
	autoconf \
	autoconf-archive \
	automake \
	bison \
	civetweb-devel \
	clang \
	clang-analyzer \
	clang18 \
	clang18-devel \
	cmake \
	cyrus-sasl-devel \
	czmq-devel \
	flex \
	gcc \
	gdb \
	git \
	gnutls-devel \
	hiredis \
	hiredis-devel \
	iproute \
	java-21-openjdk \
	java-21-openjdk-devel \
	apr-util-devel \
	libcurl-devel \
	libdbi-dbd-mysql \
	libdbi-devel \
	libfaketime \
	libgcrypt-devel \
	libmaxminddb \
	libmaxminddb-devel \
	libnet \
	libnet-devel \
	librabbitmq-devel \
	libtool \
	libuuid-devel \
	libzstd-devel \
	logrotate \
	lsof \
	make \
	mbedtls-devel \
	mongo-c-driver \
	mongo-c-driver-devel \
	mysql-devel \
	nc \
	net-snmp-devel \
	net-tools \
	openssl \
	openssl-devel \
	openssl-devel-engine \
	postgresql-devel \
	which \
	procps-ng \
	python3-devel \
	python3-docutils \
	python3-pysnmp \
	python3-sphinx \
	protobuf-c \
	protobuf-c-devel \
	qpid-proton-c-devel \
	redhat-rpm-config \
	snappy-devel \
	systemd-devel \
	tcl-devel \
	valgrind \
	wget \
	zlib-devel
	# end of this RUN
# unfortunately, tcl-devel does not properly setup required bits in the environment
# so we now try to do that. In case this does no longer work with a version, search
# for a file tclConfig.sh, which should be present in the library directory (usually
# beneath /usr). It contains the environment variables. Inside container do:
# $cat $(find /usr -name tclConfig.sh|head -n1)
ENV	TCL_LIB_SPEC="-L/usr/lib64 -ltcl8.6" \
	TCL_INCLUDE_SPEC="-I/usr/include"

WORKDIR	/home/devel
RUN	groupadd rsyslog \
	&& adduser -g rsyslog  -s /bin/bash rsyslog \
	&& echo "rsyslog ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
RUN	mkdir /rsyslog && \
	chown rsyslog:rsyslog /rsyslog
VOLUME	/rsyslog
ENV	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
	LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64 \
	LIBDIR_PATH=/usr/lib64

# bump dependency version below to trigger a dependency rebuild
# but not a full one (via --no-cache)
ENV	DEP_VERSION=3
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
	CFLAGS="$CFLAGS -Wno-calloc-transposed-args" ./configure --prefix=/usr --libdir=/usr/lib64 && \
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
	CFLAGS="$CFLAGS -Wno-calloc-transposed-args" ./configure --prefix=/usr --enable-compile-warnings=yes --libdir=/usr/lib64 && \
	make -j4 && \
	make install && \
	cd .. && \
	rm -r librelp && \
	cd ..

# we need libfaup for some modules - packages do usually not exist
RUN	cd helper-projects && \
	git clone https://github.com/stricaud/faup.git && \
	cd faup && \
	cd build && \
	cmake .. && make -j && \
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
	(unset CFLAGS; ./configure --prefix=/usr --libdir=$LIBDIR_PATH --CFLAGS="-g" ; make -j2) && \
	make install && \
	cd .. && \
# Note: we do NOT delete the source as we may need it to
# uninstall (in case the user wants to go back to system-default)
	cd ..

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
ENV SCAN_BUILD=scan-build \
    SCAN_BUILD_CC=clang18

ENV RSYSLOG_CONFIGURE_OPTIONS \
	--enable-elasticsearch \
	--enable-elasticsearch-tests \
	--disable-clickhouse \
	--disable-clickhouse-tests \
	--enable-ffaup \
	--enable-gnutls \
	--enable-gssapi-krb5 \
	# CZMQ modules disabled due to API breaking changes in CZMQ 4.2+
	# TODO: Re-enable once CZMQ 4.2+ compatibility is implemented
	# See: https://github.com/rsyslog/rsyslog/issues/5996 (create issue)
	--disable-imczmq \
	--enable-imdiag \
	--enable-imhttp \
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
	--enable-libzstd \
	--enable-mail \
	--enable-mbedtls \
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
	--enable-mmutf8fix \
	--enable-mysql \
	--enable-omamqp1 \
	--disable-omczmq \
	--enable-omhiredis \
	--enable-omhttp \
	--enable-omhttpfs \
	--enable-omjournal \
	--enable-omkafka \
	--enable-ommongodb \
	--enable-omprog \
	# RabbitMQ module disabled due to deprecated API usage in rabbitmq-c
	# TODO: Re-enable once rabbitmq-c API compatibility is updated
	# See: https://github.com/rsyslog/rsyslog/issues/5997 (create issue)
	--disable-omrabbitmq \
	--enable-omrelp-default-port=13515 \
	--enable-omruleset \
	--enable-omsendertrack \
	--enable-omstdout \
	# TCL module disabled due to missing TCL 8.6 library in Fedora 42
	# TODO: Re-enable once TCL 8.6 library is available or alternative is found
	# See: https://github.com/rsyslog/rsyslog/issues/5998 (create issue)
	--disable-omtcl \
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
	--enable-compile-warning=error \
	--enable-testbench

# build errors at the moment: --enable-kmsg 
#	--enable-mmgrok - no package

WORKDIR	/rsyslog
USER	rsyslog
