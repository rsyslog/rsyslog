# container for rsyslog development
# creates the build environment
FROM	alpine:latest
LABEL   maintainer="rgerhards@adiscon.com"
COPY	rsyslog@lists.adiscon.com-5a55e598.rsa.pub /etc/apk/keys/rsyslog@lists.adiscon.com-5a55e598.rsa.pub
RUN	echo "http://alpine.rsyslog.com/3.7/stable" >> /etc/apk/repositories \
	&& apk --no-cache update
RUN	apk add --no-cache \
		git build-base automake libtool autoconf py-docutils gnutls gnutls-dev \
		zlib-dev curl-dev mysql-dev libdbi-dev libuuid util-linux-dev \
		libgcrypt-dev flex bison bsd-compat-headers linux-headers valgrind librdkafka-dev \
		autoconf-archive \
		libestr-dev \
		libfastjson-dev \
		liblognorm-dev \
		librelp-dev \
		liblogging-dev
WORKDIR /home/ci
ADD common/setup-projects.sh /home/ci
ADD setup-system.sh setup-system.sh
ENV SUDO=
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
    LD_LIBRARY_PATH=/usr/local/lib \
    CFLAGS="-Os -fomit-frame-pointer"
ENV RSYSLOG_CONFIGURE_OPTIONS -enable-testbench --enable-imdiag --enable-imfile --enable-impstats --enable-imptcp --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omruleset --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-libgcrypt --enable-mmnormalize --disable-omudpspoof --enable-relp --disable-snmp --disable-mmsnmptrapd --enable-gnutls --enable-mysql --enable-mysql-tests --enable-usertools=no --enable-libdbi --enable-omhttpfs --enable-elasticsearch --enable-valgrind --enable-ommongodb=no --enable-imkafka --enable-omkafka --enable-pmnull

# the second step shall later go into a different container
# for now, let's get things going first... - we currently do not even need it!
#RUN ./setup-projects.sh
