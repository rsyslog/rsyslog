#!/bin/bash
# rsgtutil utility test 
#	Extract lines from sample logdata and verifies against public 
#	ksi verification repository. Test uses valgrind.
#
# This is part of the rsyslog testbench, licensed under GPLv3
#
# Copyright 2016 Rainer Gerhards and Adiscon GmbH.
RSYSLOG_KSI_BIN="http://verify.guardtime.com/ksi-publications.bin"
#RSYSLOG_KSI_DEBUG="--debug"
#RSYSLOG_KSI_LOG="ksi-sample.log"
RSYSLOG_KSI_LOG="ksi.log"

echo "[ksi-extract-verify-long-vg.sh]: testing rsgtutil extract with valgrind - long options"
. $srcdir/diag.sh init

echo "running rsgtutil extract command"
valgrind $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=full ../tools/rsgtutil $RSYSLOG_KSI_DEBUG --show-verified --extract 1,3,5 --output $srcdir/ksi-export.log --publications-server http://verify.guardtime.com/ksi-publications.bin $srcdir/testsuites/$RSYSLOG_KSI_LOG

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "[ksi-extract-verify-long-vg.sh]: rsgtutil extract failed with error: " $RSYSLOGD_EXIT
	exit 1;
fi

echo "running rsgtutil verify command"
valgrind $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=full ../tools/rsgtutil $RSYSLOG_KSI_DEBUG --verify --show-verified --publications-server http://verify.guardtime.com/ksi-publications.bin $srcdir/ksi-export.log

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "[ksi-extract-verify-long-vg.sh]: rsgtutil verify failed with error: " $RSYSLOGD_EXIT
	exit 1;
fi

# Cleanup temp files
rm -f $srcdir/ksi-export.*

echo SUCCESS: rsgtutil extract with valgrind - long options