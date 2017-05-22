# this installs some components that we cannot install any other way
source /etc/lsb-release
# the following packages are not yet available via travis package
sudo apt-get install -qq faketime libdbd-mysql libmongo-client-dev autoconf-archive
if [ "x$GROK" == "xYES" ]; then sudo apt-get install -qq libgrok1 libgrok-dev ; fi
sudo apt-get install -qq --force-yes libestr-dev librelp-dev libfastjson-dev liblogging-stdlog-dev libksi1 libksi1-dev \
	liblognorm-dev \
	libcurl4-gnutls-dev
sudo apt-get install -qq python-docutils

if [ "$DISTRIB_CODENAME" == "trusty" ] || [ "$DISTRIB_CODENAME" == "precise" ]; then
	set -ex
	WANT_MAXMIND=1.2.0
	curl -Ls https://github.com/maxmind/libmaxminddb/releases/download/${WANT_MAXMIND}/libmaxminddb-${WANT_MAXMIND}.tar.gz | tar -xz
	(cd libmaxminddb-${WANT_MAXMIND} ; ./configure --prefix=/usr CC=gcc CFLAGS="-Wall -Wextra -g -pipe -std=gnu99"  > /dev/null ; sudo make install &> /dev/null)
	set +x
else
	sudo apt-get install -qq libmaxminddb-dev
fi

# As travis has no xenial images, we always need to install librdkafka from source
if [ "x$KAFKA" == "xYES" ]; then 
	set -ex
	git clone https://github.com/edenhill/librdkafka
	echo $CFLAGS
	(unset CFLAGS; cd librdkafka ; ./configure --prefix=/usr --CFLAGS="-g" > /dev/null ; make  > /dev/null ; sudo make install > /dev/null)
	find /usr -name rdkafka.h
	find /usr -name rdkafka.pc
	ls -l /usr/lib/pkgconfig
	cat /usr/include/librdkafka/rdkafka.h
	set +x
fi
#if [ "x$KAFKA" == "xYES" ]; then sudo apt-get install -qq librdkafka-dev ; fi

if [ "x$ESTEST" == "xYES" ]; then sudo apt-get install -qq elasticsearch ; fi
if [ "$DISTRIB_CODENAME" == "trusty" ]; then sudo apt-get install -qq libhiredis-dev; export HIREDIS_OPT="--enable-omhiredis"; fi
if [ "$DISTRIB_CODENAME" == "trusty" ]; then sudo apt-get install -qq libsystemd-journal-dev; export JOURNAL_OPT="--enable-imjournal --enable-omjournal"; fi
if [ "$DISTRIB_CODENAME" != "precise" ]; then sudo apt-get install -qq --force-yes libqpid-proton3-dev ;fi
if [ "$CC" == "clang" ] && [ "$DISTRIB_CODENAME" == "trusty" ]; then CLANG_PKG="clang-3.6"; SCAN_BUILD="scan-build-3.6"; else CLANG_PKG="clang"; SCAN_BUILD="scan-build"; fi
if [ "$CC" == "clang" ]; then export NO_VALGRIND="--without-valgrind-testbench"; fi
if [ "$CC" == "clang" ]; then sudo apt-get install -qq $CLANG_PKG ; fi
if [ "x$KAFKA" == "xYES" ]; then export ENABLE_KAFKA="--enable-omkafka --enable-imkafka --enable-kafka-tests" ; fi
