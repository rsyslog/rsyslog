#!/bin/bash
# rsgtutil utility test 
#	Verifies sample logdata against public ksi verification repository. 
#	Test uses valgrind.
#
# This file is part of the rsyslog project, released under ASL 2.0
#
# Copyright 2016 Rainer Gerhards and Adiscon GmbH.
RSYSLOG_KSI_BIN="http://verify.guardtime.com/ksi-publications.bin"
RSYSLOG_KSI_DEBUG="--debug"
RSYSLOG_KSI_LOG="ksi-sample.log"

echo \[ksi-verify-long-vg.sh\]: testing rsgtutil verify with valgrind - long options
. $srcdir/diag.sh init

echo "running rsgtutil command with long options"
valgrind $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=full ../tools/rsgtutil $RSYSLOG_KSI_DEBUG --verify --show-verified --publications-server $RSYSLOG_KSI_BIN $srcdir/testsuites/$RSYSLOG_KSI_LOG

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "rsgtutil returned error: " $RSYSLOGD_EXIT
	exit 1;
fi

# Cleanup temp files
rm -f rsgtutil.out*.log 

echo SUCCESS: rsgtutil verify function with valgrind- long options