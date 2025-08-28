#!/bin/bash
# this script runs the travis CI testbench. It's easier and more
# powerful this way than using .travis.yml (plus recommended by travis support)
#
# THIS IS THE OSX VERSION OF THAT SCRIPT
#
# This environment is so different that it does not make sense to do
# both osx and linux in a single script.
#
#set -v  # we want to see the execution steps
set -e  # abort on first failure
#set -x  # debug aid

echo "****************************** BEGIN ACTUAL SCRIPT STEP ******************************"
echo "OS:               $TRAVIS_OS_NAME"
echo "DISTRIB_CODENAME: $DISTRIB_CODENAME"
echo "CLANG:            $CLANG"
echo "PWD:              $PWD"


export PKG_CONFIG_PATH="/opt/rsyslog/lib/pkgconfig"
source tests/CI/prep-liblogging.sh
source tests/CI/prep-libestr.sh
source tests/CI/prep-libfastjson.sh

echo "****************************** END PREP STEP ******************************"

# we turn off leak sanitizer at this time because it reports some
# pretty irrelevant problems in startup code. In the longer term,
# we should clean these up, but we also have a lot of other leak
# tests, so this is not our priority at the moment (much more
# important things are on the TODO list).
export ASAN_OPTIONS=detect_leaks=0

autoreconf --force --verbose --install
export CONFIG_FLAGS="--prefix=/opt/rsyslog --enable-silent-rules --enable-testbench --enable-imbatchreport --enable-imdiag --enable-imfile --enable-improg --enable-impstats --enable-mmrm1stspace --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-mmcount --disable-omudpspoof --enable-snmp --enable-mmsnmptrapd --disable-uuid --disable-libgcrypt \
	--enable-pmnull \
	--enable-pmnormalize=no \
	--enable-imdocker \
	--disable-generate-man-pages"
./configure  $CONFIG_FLAGS
export USE_AUTO_DEBUG="off" # set to "on" to enable this for travis
make -j

if [ "x$CHECK" == "xYES" ]
then
    set +e  # begin testbench, here we do not want to abort
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
    set -e # now errors are no longer permitted, again
    make distcheck
fi
