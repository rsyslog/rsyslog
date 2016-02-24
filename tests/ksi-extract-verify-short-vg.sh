#!/bin/bash
# rsgtutil utility test 
#	Extract lines from sample logdata and verifies against public 
#	ksi verification repository. Test uses valgrind.
#
# This is part of the rsyslog testbench, licensed under GPLv3
#
# Copyright 2016 Rainer Gerhards and Adiscon GmbH.
RSYSLOG_KSI_BIN="http://verify.guardtime.com/ksi-publications.bin"

echo "[ksi-extract-verify-short-vg.sh]: testing rsgtutil extract with valgrind - short options"
. $srcdir/diag.sh init

echo "running rsgtutil extract command"
valgrind $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=full ../tools/rsgtutil -s -x 1,3,5 -o $srcdir/ksi-export.log -P http://verify.guardtime.com/ksi-publications.bin $srcdir/testsuites/ksi-sample.log

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "[ksi-extract-verify-short-vg.sh]: rsgtutil extract failed with error: " $RSYSLOGD_EXIT
	exit 1;
fi

echo "running rsgtutil verify command"
valgrind $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=full ../tools/rsgtutil -t -s -P http://verify.guardtime.com/ksi-publications.bin $srcdir/ksi-export.log

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "[ksi-extract-verify-short-vg.sh]: rsgtutil verify failed with error: " $RSYSLOGD_EXIT
	exit 1;
fi

# Cleanup temp files
rm -f $srcdir/ksi-export.*

echo SUCCESS: rsgtutil extract with valgrind - short options