# note: repository key is added in Dockerfile!
echo "http://build.rsyslog.com/alpine/3.7/unstable" >> /etc/apk/repositories
apk --no-cache update
apk add --no-cache \
	git build-base automake libtool autoconf py-docutils gnutls gnutls-dev \
	zlib-dev curl-dev mysql-dev libdbi-dev libuuid util-linux-dev \
	libgcrypt-dev flex bison bsd-compat-headers linux-headers valgrind librdkafka-dev \
	autoconf-archive

# interactive dev only:
apk add man man-pages gcc-doc
