#!/bin/bash
# this installs some components that we cannot install any other way
source /etc/lsb-release

if [ "${AD_PPA}x" == "x" ] ; then AD_PPA="v8-stable"; fi
sudo add-apt-repository ppa:adiscon/$AD_PPA -y
sudo apt-get install -qq faketime libdbd-mysql
sudo add-apt-repository ppa:qpid/released -y
sudo apt-get update

if [ "$DISTRIB_CODENAME" != "xenial" ]; then
	# update autoconf-archive (no good enough packets available)
	# this one built by whissi
	wget --no-verbose http://build.rsyslog.com/CI/autoconf-archive_20170928-1adiscon1_all.deb
	if [ $? -ne 0 ]; then
		echo Download autoconf-archive failed!
		exit 1
	fi
	sudo dpkg -i autoconf-archive_20170928-1adiscon1_all.deb
	rm autoconf-archive_20170928-1adiscon1_all.deb

	sudo apt-get install openjdk-7-jdk
fi
if [ "$DISTRIB_CODENAME" == "xenial" ]; then
	sudo apt-get install libgnutls28-dev
fi
sudo apt-get install build-essential automake pkg-config libtool autoconf autotools-dev gdb valgrind libdbi-dev libsnmp-dev libmysqlclient-dev postgresql-client libglib2.0-dev libtokyocabinet-dev zlib1g-dev uuid-dev libgcrypt11-dev bison flex libcurl4-openssl-dev python-docutils wget libkrb5-dev libnet1-dev

if [ "x$GROK" == "xYES" ]; then sudo apt-get install -qq libgrok1 libgrok-dev ; fi
sudo apt-get install -qq --force-yes libestr-dev librelp-dev libfastjson-dev liblogging-stdlog-dev \
	liblognorm-dev \
	libcurl4-openssl-dev
sudo apt-get install -qq python-docutils

if [ "$DISTRIB_CODENAME" == "trusty" ] || [ "$DISTRIB_CODENAME" == "precise" ]; then
	WANT_MAXMIND=1.2.0
	curl -Ls https://github.com/maxmind/libmaxminddb/releases/download/${WANT_MAXMIND}/libmaxminddb-${WANT_MAXMIND}.tar.gz | tar -xz
	(cd libmaxminddb-${WANT_MAXMIND} ; ./configure --prefix=/usr CC=gcc CFLAGS="-Wall -Wextra -g -pipe -std=gnu99"  > /dev/null ; make -j2  &>/dev/null ; sudo make install > /dev/null)
	rm -rf libmaxminddb-${WANT_MAXMIND} # get rid of source, e.g. for line length check
	
	SAVE_CFLAGS=$CFLAGS
	CFLAGS=""
	SAVE_CC=$CC
	CC="gcc"
	sudo apt-get install -qq libssl-dev libsasl2-dev
	wget --no-verbose https://github.com/mongodb/mongo-c-driver/releases/download/1.8.1/mongo-c-driver-1.8.1.tar.gz
	tar -xzf mongo-c-driver-1.8.1.tar.gz
	cd mongo-c-driver-1.8.1/
	./configure --enable-ssl --disable-automatic-init-and-cleanup > /dev/null
	make -j > /dev/null
	sudo make install > /dev/null
	cd -
	rm -rf mongo-c-driver-1.8.1
	CC=$SAVE_CC
	CFLAGS=$SAVE_CFLAGS
else
	sudo apt-get install -qq libmaxminddb-dev libmongoc-dev
fi

# As travis has no xenial images, we always need to install librdkafka from source
if [ "x$KAFKA" == "xYES" ]; then 
	sudo apt-get install -qq liblz4-dev 
	# For kafka testbench, "kafkacat" package is needed!
	git clone https://github.com/edenhill/librdkafka > /dev/null
	(unset CFLAGS; cd librdkafka ; ./configure --prefix=/usr --CFLAGS="-g" > /dev/null ; make -j2  > /dev/null ; sudo make install > /dev/null)
	rm -rf librdkafka # get rid of source, e.g. for line length check
fi

if [ "x$GCC" == "xNEWEST" ]; then
	# currently the best repo we can find...
	sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
	sudo apt-get update -y -qq
	sudo apt-get install gcc-7 -y -qq
	export CC=gcc-7
fi

if [ "x$ESTEST" == "xYES" ]; then sudo apt-get install -qq elasticsearch ; fi
if [ "$DISTRIB_CODENAME" == "trusty" ]; then sudo apt-get install -qq libhiredis-dev; export HIREDIS_OPT="--enable-omhiredis"; fi
if [ "$DISTRIB_CODENAME" == "trusty" ]; then sudo apt-get install -qq libsystemd-journal-dev; export JOURNAL_OPT="--enable-imjournal --enable-omjournal"; fi
if [ "$DISTRIB_CODENAME" == "trusty" ] || [ "$DISTRIB_CODENAME" == "precise" ]; then
	export IMDOCKER_OPT=
else
	if [ "x$IMDOKER" == "xYES" ]; then
#    git clone https://github.com/curl/curl.git > /dev/null
#    (cd curl; ./buildconf; ./configure; make -j2 > /dev/null; sudo make install > /dev/null;)
		export IMDOCKER_OPT="--enable-imdocker --enable-imdocker-tests";
	fi
fi
if [ "$DISTRIB_CODENAME" == "trusty" ]; then sudo apt-get install -qq --force-yes libqpid-proton3-dev ;fi
if [ "$CC" == "clang" ] && [ "$DISTRIB_CODENAME" == "trusty" ]; then sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
sudo bash -c "echo \"deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-5.0 main\" > /etc/apt/sources.list.d/llvm.list" &&\
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key|sudo apt-key add - &&\
sudo apt-get update -yy  &&\
sudo apt-get install clang-5.0 clang-tools-5.0; fi

if [ "$CC" == "clang" ] && [ "$DISTRIB_CODENAME" == "trusty" ]; then export CLANG_PKG="clang-5.0"; export SCAN_BUILD="scan-build-5.0"; else export CLANG_PKG="clang"; export SCAN_BUILD="scan-build"; fi
if [ "$CC" == "clang" ]; then export NO_VALGRIND="--without-valgrind-testbench"; fi
if [ "$CC" == "clang" ]; then sudo apt-get install -qq $CLANG_PKG ; fi


if [ "x$KAFKA" == "xYES" ]; then export ENABLE_KAFKA="--enable-omkafka --enable-imkafka --enable-kafka-tests=no" ; fi
if [ "x$DEBUGLESS" == "xYES" ]; then export ENABLE_DEBUGLESS="--enable-debugless" ; fi

#
# uncomment the following if AND ONLY If you need yet-unreleased packages
# from the v8-devel repo. Be sure to reset this once the new release has
# been crafted!
#sudo add-apt-repository ppa:adiscon/v8-devel -y
#sudo apt-get update
# now come the actual overrides
#sudo apt-get install libfastjson-dev
#

# end package override code
