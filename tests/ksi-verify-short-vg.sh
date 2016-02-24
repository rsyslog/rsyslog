#!/bin/bash
# rsgtutil configuration test 
#	Verifies sample logdata against public ksi verification repository. 
#
# This is part of the rsyslog testbench, licensed under GPLv3
#
# Copyright 2016 Rainer Gerhards and Adiscon GmbH.
RSYSLOG_KSI_BIN="http://verify.guardtime.com/ksi-publications.bin"

echo \[ksi-verify-short-vg.sh\]: testing rsgtutil verify with valgrind - short options
. $srcdir/diag.sh init

echo "running rsgtutil command with short options"
valgrind $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=full ../tools/rsgtutil -t -s -P $RSYSLOG_KSI_BIN $srcdir/testsuites/ksi-sample.log #> rsgtutil.out.log 2>&1

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "rsgtutil returned error: " $RSYSLOGD_EXIT
	exit 1;
fi

# Cleanup temp files
rm -f rsgtutil.out*.log 

echo SUCCESS: rsgtutil verify function with valgrind - short options