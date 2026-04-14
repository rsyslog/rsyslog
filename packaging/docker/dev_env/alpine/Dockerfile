# container for rsyslog development
# creates the build environment
FROM	alpine:3.23
LABEL   maintainer="rgerhards@adiscon.com"
RUN	apk --no-cache update && \
	apk add --no-cache \
		autoconf \
		autoconf-archive \
		automake \
		bison \
		build-base \
		bash \
		bsd-compat-headers \
		curl-dev \
		flex \
		git \
		gnutls-dev \
		libdbi-dev \
		libestr-dev \
		libfastjson-dev \
		libgcrypt-dev \
		liblognorm-dev \
		librelp-dev \
		libtool \
		libuuid \
		linux-headers \
		librdkafka-dev \
		mariadb-dev \
		pkgconf \
		python3 \
		py3-docutils \
		protobuf-c-compiler \
		protobuf-c-dev \
		snappy-dev \
		util-linux-dev \
		valgrind \
		yaml-dev \
		zlib-dev
COPY	rsyslog@lists.adiscon.com-5a55e598.rsa.pub /etc/apk/keys/rsyslog@lists.adiscon.com-5a55e598.rsa.pub
WORKDIR /rsyslog
ADD common/setup-projects.sh /home/ci
ADD setup-system.sh setup-system.sh
ENV SUDO=
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
    LD_LIBRARY_PATH=/usr/local/lib \
    CFLAGS="-Os -fomit-frame-pointer"
ENV RSYSLOG_CONFIGURE_OPTIONS="\
--enable-testbench \
--enable-imdiag \
--enable-imfile \
--enable-impstats \
--enable-imptcp \
--enable-mmjsonparse \
--enable-mmutf8fix \
--enable-omstdout \
--enable-omuxsock \
--enable-gnutls \
--enable-libgcrypt \
--enable-libdbi \
--enable-mysql \
--enable-relp \
--enable-mmnormalize \
--enable-imkafka \
--enable-omkafka \
--enable-valgrind \
--enable-usertools=no \
--disable-omudpspoof \
--disable-snmp \
--disable-mmsnmptrapd \
--disable-elasticsearch-tests \
--disable-ommongodb \
--enable-pmnull"

# the second step shall later go into a different container
# for now, let's get things going first... - we currently do not even need it!
#RUN ./setup-projects.sh
