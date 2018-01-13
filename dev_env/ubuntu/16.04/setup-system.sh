export DEBIAN_FRONTEND=noninteractive
set -v
apt-get update -y

echo STEP: essentials
apt-get install -y software-properties-common python-software-properties wget

echo STEP: set gcc-7 repo
# currently the best repo for gcc-7 we can find...
add-apt-repository ppa:qpid/released -y
add-apt-repository ppa:ubuntu-toolchain-r/test -y

# LLVM repository
echo STEP: set clang 5 repo
echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-5.0 main" > /etc/apt/sources.list.d/llvm.list
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key| apt-key add -

# 0mq repository
echo STEP: set 0mq repo
echo "deb http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_16.04/ ./" > /etc/apt/sources.list.d/0mq.list
wget -O - http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_12.04/Release.key | apt-key add -

# Ready to go
apt-get update -y
apt-get upgrade -y
echo STEP: install main components
apt-get install -y \
	vim \
	gcc-7 \
	mysql-server \
	pkg-config \
	libtool \
	autoconf \
	autotools-dev \
	automake \
	python-docutils \
	pkg-config \
	libtool \
	gdb \
	valgrind \
	libdbi-dev \
	libczmq-dev \
	libsnmp-dev \
	libmysqlclient-dev \
	libglib2.0-dev \
	libtokyocabinet-dev \
	zlib1g-dev \
	uuid-dev \
	libgcrypt11-dev \
	bison \
	flex \
	clang \
	clang-5.0 clang-tools-5.0 \
	libcurl4-gnutls-dev \
	python-docutils  \
	wget \
	libgnutls28-dev \
	libsystemd-dev \
	libhiredis-dev \
	librdkafka-dev  \
	libnet1-dev \
	postgresql-client libpq-dev \
	libgrok1 libgrok-dev \
	libmongoc-dev \
	libbson-dev \
	git \
	faketime libdbd-mysql autoconf-archive \
	libssl-dev libsasl2-dev \
	libmaxminddb-dev libmongoc-dev \
	liblz4-dev \
	libqpid-proton10-dev \
	libkrb5-dev \
	libsodium-dev

# Whissi "special build" of 2017-12 autoconf-archive:
wget --no-verbose http://build.rsyslog.com/CI/autoconf-archive_20170928-1adiscon1_all.deb
#dpkg -i autoconf-archive_20170928-1adiscon1_all.deb
# BAD work-around
apt-get install -y autoconf-archive
rm autoconf-archive_20170928-1adiscon1_all.deb

# Now build some custom libraries for whom there are not packages
mkdir helper-projects
cd helper-projects

# we need Guardtime libksi here, otherwise we cannot check the KSI component
git clone https://github.com/guardtime/libksi.git
cd libksi
autoreconf -fvi
./configure --prefix=/usr
make -j2
make install
cd ..
rm -r libksi

# librdkafka --> source
cd ..

# finished installing helpers from source
rm -r helper-projects

