#!/bin/bash
# rsgtutil utility test 
#	Extract lines from sample logdata and verifies against public 
#	ksi verification repository. 
#
# This is part of the rsyslog testbench, licensed under GPLv3
#
# Copyright 2016 Rainer Gerhards and Adiscon GmbH.
RSYSLOG_KSI_BIN="http://verify.guardtime.com/ksi-publications.bin"

echo "[ksi-extract-verify-long.sh]: testing rsgtutil extract function - long options"
. $srcdir/diag.sh init

echo "running rsgtutil extract command"
../tools/rsgtutil --show-verified --extract 1,3,5 --output $srcdir/ksi-export.log --publications-server http://verify.guardtime.com/ksi-publications.bin $srcdir/testsuites/ksi-sample.log

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "[ksi-extract-verify-long.sh]: rsgtutil extract failed with error: " $RSYSLOGD_EXIT
	exit 1;
fi

echo "running rsgtutil verify command"
../tools/rsgtutil --verify --show-verified --publications-server http://verify.guardtime.com/ksi-publications.bin $srcdir/ksi-export.log

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "[ksi-extract-verify-long.sh]: rsgtutil verify failed with error: " $RSYSLOGD_EXIT
	exit 1;
fi

# Cleanup temp files
rm -f $srcdir/ksi-export.*

echo SUCCESS: rsgtutil extract function - long options