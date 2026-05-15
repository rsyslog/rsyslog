#!/bin/bash
cd /rsyslog
set -e
echo "SCAN_BUILD_CC: $SCAN_BUILD_CC"
echo "SCAN_BUILD: $SCAN_BUILD"
echo "CI_MAKE_OPT: ${CI_MAKE_OPT:--j2}"

if [ -n "$SCAN_BUILD_REPORT_DIR" ]
then
  export CURR_REPORT=$(date +%y-%m-%d_%H-%M-%S)
  export REPORT_DIR="$SCAN_BUILD_REPORT_DIR/$CURR_REPORT"
  export REPORT_OPT="-o $REPORT_DIR"
fi

autoreconf -fvi

if [ -n "$CI_CONFIGURE_CACHE" ]
then
  if [ -z "$CI_CONFIGURE_CACHE_FILE" ]
  then
    CI_CONFIGURE_CACHE_FILE=config.cache
  fi
  CONFIGURE_CACHE_OPTS="--cache-file=$CI_CONFIGURE_CACHE_FILE"
else
  CONFIGURE_CACHE_OPTS=
fi

export CC=$SCAN_BUILD_CC
./configure $CONFIGURE_CACHE_OPTS $RSYSLOG_CONFIGURE_OPTIONS $RSYSLOG_CONFIGURE_OPTIONS_EXTRA

set +e
$SCAN_BUILD $REPORT_OPT --use-cc $SCAN_BUILD_CC --status-bugs make ${CI_MAKE_OPT:--j2}

RESULT=$?
set -e

if [ $RESULT -eq 1 ]
then
   echo scan-build failed
   if [ -n "$SCAN_BUILD_REPORT_DIR" ]
   then
      echo "scan-build report URL: ${SCAN_BUILD_REPORT_BASEURL}${CURR_REPORT}" > report_url
   fi
fi
#make clean
echo static analyzer result: $RESULT
exit $RESULT
