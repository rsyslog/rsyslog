# this installs some components that we cannot install any other way
source /etc/lsb-release
# the following packages are not yet available via travis package
sudo apt-get install -qq faketime libdbd-mysql libmongo-client-dev
if [ "x$GROK" == "xYES" ]; then sudo apt-get install -qq libgrok1 libgrok-dev ; fi
sudo apt-get install -qq libestr-dev librelp-dev libfastjson-dev liblogging-stdlog-dev libksi1 libksi1-dev
sudo apt-get install -qq python-docutils liblognorm1-dev 
if [ "$DISTRIB_CODENAME" == "trusty" ]; then sudo apt-get install -qq libhiredis-dev; export HIREDIS_OPT="--enable-omhiredis"; fi
if [ "$DISTRIB_CODENAME" == "trusty" ]; then sudo apt-get install -qq libsystemd-journal-dev; export JOURNAL_OPT="--enable-imjournal --enable-omjournal"; fi
sudo apt-get install -qq libqpid-proton3-dev
if [ "$CC" == "clang" ] && [ "$DISTRIB_CODENAME" == "trusty" ]; then CLANG_PKG="clang-3.6"; SCAN_BUILD="scan-build-3.6"; else CLANG_PKG="clang"; SCAN_BUILD="scan-build"; fi
if [ "$CC" == "clang" ]; then export NO_VALGRIND="--without-valgrind-testbench"; fi
if [ "$CC" == "clang" ]; then sudo apt-get install -qq $CLANG_PKG ; fi
if [ "x$KAFKA" == "xYES" ]; then sudo apt-get install -qq librdkafka-dev ; fi
if [ "x$KAFKA" == "xYES" ]; then export ENABLE_KAFKA="--enable-omkafka" ; fi
