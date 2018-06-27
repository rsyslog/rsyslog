# this script runs the travis CI testbench. It's easier and more
# powerful this way than using .travis.yml (plus recommended by travis support)
#
set -v  # we want to see the execution steps
set -e  # abort on first failure
#set -x  # debug aid

echo "DISTRIB_CODENAME: $DISTRIB_CODENAME"
echo "CLANG:            $CLANG"
echo "****************************** BEGIN ACTUAL SCRIPT STEP ******************************"

source tests/travis/install.sh
source /etc/lsb-release

#
# ACTUAL MAIN CI PART OF THE SCRIPT
# This is to be executed for each PR
#

# we turn off leak sanitizer at this time because it reports some
# pretty irrelevant problems in startup code. In the longer term,
# we should clean these up, but we also have a lot of other leak
# tests, so this is not our priority at the moment (much more
# important things are on the TODO list).
export ASAN_OPTIONS=detect_leaks=0

if [ "$MERGE" == "YES" ]; then
    # we need to use source as we must exit on inability to merge!
    set +v
    set +e
    source tests/CI/try_merge.sh --merge-only
    set -v
    set -e
fi

set -e
if [ "$CC" == "clang" ] && [ "$DISTRIB_CODENAME" == "trusty" ]; then SCAN_BUILD="scan-build-5.0"; CC=clang-5.0; else SCAN_BUILD="scan-build"; fi

ls -l *.tar.gz
rm -f *.tar.gz # safety check: we must not have tarballs at this stage
if [ "x$BUILD_FROM_TARBALL" == "xYES" ]; then
	autoreconf -fvi
	./configure
	make dist
	ls -l *.tar.gz
	mv *.tar.gz rsyslog.tar.gz
	mkdir unpack
	cd unpack
	tar xzf ../rsyslog.tar.gz
	ls -ld rsyslog*
	cd rsyslog*
fi
pwd
autoreconf --force --verbose --install
if [ "x$GROK" == "xYES" ]; then export GROK="--enable-mmgrok"; fi
if [ "x$ESTEST" == "xYES" ]; then export ES_TEST_CONFIGURE_OPT="--enable-elasticsearch-tests=minimal" ; fi
# at this point, the environment should be setup for ./configure
if [ "$CC" == "clang" ] && [ "$DISTRIB_CODENAME" == "trusty" ]; then export CC="clang-3.6"; fi
$CC -v

if [ "$DISTRIB_CODENAME" != "precise" ]; then AMQP1="--enable-omamqp1"; fi
export CONFIG_FLAGS="$CONFIGURE_FLAGS \
	$EXTRA_CONFIGURE \
	$JOURNAL_OPT \
	$HIREDIS_OPT \
	$ENABLE_KAFKA \
	$ENABLE_DEBUGLESS \
	$NO_VALGRIND \
	$GROK \
	$ES_TEST_CONFIGURE_OPT \
	$AMQP1 \
	--disable-generate-man-pages \
	--enable-distcheck-workaround \
	--enable-testbench \
	--enable-imdiag \
	--enable-imfile \
	--enable-impstats \
	--enable-mmrm1stspace \
	--enable-imptcp \
	--enable-mmanon \
	--enable-mmaudit \
	--enable-mmfields \
	--enable-mmjsonparse \
	--enable-mmpstrucdata \
	--enable-mmsequence \
	--enable-mmutf8fix \
	--enable-mail \
	--enable-omprog \
	--enable-omruleset \
	--enable-omstdout \
	--enable-omuxsock \
	--enable-pmaixforwardedfrom \
	--enable-pmciscoios \
	--enable-pmcisconames \
	--enable-pmlastmsg \
	--enable-pmsnare \
	--enable-libgcrypt \
	--enable-mmnormalize \
	--enable-omudpspoof \
	--enable-relp --enable-omrelp-default-port=13515 \
	--enable-snmp \
	--enable-mmsnmptrapd \
	--enable-gnutls \
	--enable-openssl \
	--enable-mysql --enable-mysql-tests \
	--enable-gt-ksi \
	--enable-libdbi \
	--enable-pgsql --enable-pgsql-tests \
	--enable-omhttpfs \
	--enable-elasticsearch \
	--enable-valgrind \
	--enable-ommongodb \
	--enable-omtcl \
	--enable-mmdblookup \
	--enable-mmcount \
	--enable-gssapi-krb5 \
	--enable-omhiredis \
	--enable-imczmq --enable-omczmq \
	--enable-usertools \
	--enable-pmnull \
	--enable-pmnormalize"
# Note: [io]mzmq3 cannot be built any longer, according to Brian Knox they require an
# outdated version of the client lib. So we do not bother any longer about them.
./configure  $CONFIG_FLAGS
export USE_AUTO_DEBUG="off" # set to "on" to enable this for travis
make -j

if [ "x$CHECK" == "xYES" ]
then
    set +e  # begin testbench, here we do not want to abort
    devtools/prep-mysql-db.sh  # prepare mysql for testbench
    make check
    ALL_OK=$?
    if [ -f tests/test-suite.log ]
    then
        cat tests/test-suite.log
    fi
    if [ $ALL_OK -ne 0 ]
    then
        echo "error in make check, error-terminating now"
        exit $ALL_OK
    fi
    set -e # now errors are no longer permited, again
    echo now running \"make distcheck\"
    make distcheck
fi

if [ "x$STAT_AN" == "xYES" ] ; then make clean; CFLAGS="-O2"; ./configure $CONFIG_FLAGS ; fi
if [ "x$STAT_AN" == "xYES" ] ; then $SCAN_BUILD --use-cc $CC --status-bugs make -j ; fi
