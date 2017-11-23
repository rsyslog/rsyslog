# this installs some components that we cannot install any other way
source /etc/lsb-release
# the following packages are not yet available via travis package
sudo apt-get install -qq faketime libdbd-mysql autoconf-archive
if [ "x$GROK" == "xYES" ]; then sudo apt-get install -qq libgrok1 libgrok-dev ; fi
sudo apt-get install -qq --force-yes libestr-dev librelp-dev libfastjson-dev liblogging-stdlog-dev \
	liblognorm-dev \
	libcurl4-gnutls-dev
sudo apt-get install -qq python-docutils

if [ "$DISTRIB_CODENAME" == "trusty" ] || [ "$DISTRIB_CODENAME" == "precise" ]; then
	set -ex
	WANT_MAXMIND=1.2.0
	curl -Ls https://github.com/maxmind/libmaxminddb/releases/download/${WANT_MAXMIND}/libmaxminddb-${WANT_MAXMIND}.tar.gz | tar -xz
	(cd libmaxminddb-${WANT_MAXMIND} ; ./configure --prefix=/usr CC=gcc CFLAGS="-Wall -Wextra -g -pipe -std=gnu99"  > /dev/null ; make -j2  >/dev/null ; sudo make install > /dev/null)
	rm -rf libmaxminddb-${WANT_MAXMIND} # get rid of source, e.g. for line length check
	
	SAVE_CFLAGS=$CFLAGS
	CFLAGS=""
	SAVE_CC=$CC
	CC="gcc"
	sudo apt-get install -qq libssl-dev libsasl2-dev
	wget https://github.com/mongodb/mongo-c-driver/releases/download/1.8.1/mongo-c-driver-1.8.1.tar.gz
	tar -xzf mongo-c-driver-1.8.1.tar.gz
	cd mongo-c-driver-1.8.1/
	./configure --enable-ssl --disable-automatic-init-and-cleanup
	make -j
	sudo make install
	cd -
	rm -rf mongo-c-driver-1.8.1
	CC=$SAVE_CC
	CFLAGS=$SAVE_CFLAGS
	set +x
else
	sudo apt-get install -qq libmaxminddb-dev libmongoc-dev
fi

# As travis has no xenial images, we always need to install librdkafka from source
if [ "x$KAFKA" == "xYES" ]; then 
	sudo apt-get install -qq liblz4-dev
	git clone https://github.com/edenhill/librdkafka
	(unset CFLAGS; cd librdkafka ; ./configure --prefix=/usr --CFLAGS="-g" > /dev/null ; make -j2  > /dev/null ; sudo make install > /dev/null)
	rm -rf librdkafka # get rid of source, e.g. for line length check
fi
#if [ "x$KAFKA" == "xYES" ]; then sudo apt-get install -qq librdkafka-dev ; fi

if [ "x$ESTEST" == "xYES" ]; then sudo apt-get install -qq elasticsearch ; fi
if [ "$DISTRIB_CODENAME" == "trusty" ]; then sudo apt-get install -qq libhiredis-dev; export HIREDIS_OPT="--enable-omhiredis"; fi
if [ "$DISTRIB_CODENAME" == "trusty" ]; then sudo apt-get install -qq libsystemd-journal-dev; export JOURNAL_OPT="--enable-imjournal --enable-omjournal"; fi
if [ "$DISTRIB_CODENAME" != "precise" ]; then sudo apt-get install -qq --force-yes libqpid-proton3-dev ;fi
if [ "$CC" == "clang" ] && [ "$DISTRIB_CODENAME" == "trusty" ]; then sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
sudo bash -c "echo \"deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-5.0 main\" > /etc/apt/sources.list.d/llvm.list" &&\
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key|sudo apt-key add - &&\
sudo apt update -yy  &&\
sudo apt install clang-5.0; fi

if [ "$CC" == "clang" ] && [ "$DISTRIB_CODENAME" == "trusty" ]; then CLANG_PKG="clang-5.0"; SCAN_BUILD="scan-build-5.0"; else CLANG_PKG="clang"; SCAN_BUILD="scan-build"; fi
if [ "$CC" == "clang" ]; then export NO_VALGRIND="--without-valgrind-testbench"; fi
if [ "$CC" == "clang" ]; then sudo apt-get install -qq $CLANG_PKG ; fi


if [ "x$KAFKA" == "xYES" ]; then export ENABLE_KAFKA="--enable-omkafka --enable-imkafka --enable-kafka-tests" ; fi
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
