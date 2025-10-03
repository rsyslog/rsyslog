#!/bin/bash
# set $SCANBUILD_EXTRA_MODULES to specify extra configure options
# this especially helps with buildbot slaves which do have some
# exotic dependencies installed
#set -e
#set -v

export CC=clang-5.0
export SCANBUILD="scan-build-5.0 --use-cc $CC"
#export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"
echo "CC $CC, SCANBUILD $SCANBUILD"

if [ -n "$SCAN_BUILD_REPORT_DIR" ]
then
  export CURR_REPORT=`date +%y-%m-%d_%H-%M-%S`
  export REPORT_DIR="$SCAN_BUILD_REPORT_DIR/$CURR_REPORT"
  export REPORT_OPT="-o $REPORT_DIR"
fi
env | grep REPORT
$SCANBUILD ./configure --disable-Werror --disable-generate-man-pages --enable-debug --enable-testbench --enable-imdiag --enable-imfile --enable-impstats --enable-imptcp --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmjsontransform --enable-mmjsonrewrite --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omruleset --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-libgcrypt --enable-mmnormalize --disable-omudpspoof --enable-relp --disable-snmp --disable-mmsnmptrapd --enable-gnutls --enable-usertools=no --enable-mysql --enable-valgrind --enable-omjournal --enable-omczmq --enable-imczmq --enable-elasticsearch --enable-omhttp $SCANBUILD_EXTRA_MODULES
make clean
# we don't want to run the testbench, but we want to build testbench tools
# make check currently produces issues
# $SCANBUILD make check TESTS="" -j
#$SCANBUILD -o /tmp/scan-build-results --status-bugs make -j4
$SCANBUILD $REPORT_OPT --status-bugs make -j4
RESULT=$?
echo scan-build result: $RESULT
if [ $RESULT -eq 1 ]
then
   if [ -n "$SCAN_BUILD_REPORT_DIR" ]
   then
      echo "scan-build report URL: ${SCAN_BUILD_REPORT_BASEURL}${CURR_REPORT}" > report_url
   fi
fi
ls -l $REPORT_DIR
exit $RESULT
