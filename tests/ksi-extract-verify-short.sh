#!/bin/bash
# rsgtutil utility test 
#	Extract lines from sample logdata and verifies against public 
#	ksi verification repository. 
#
# This file is part of the rsyslog project, released under ASL 2.0
#
# Copyright 2016 Rainer Gerhards and Adiscon GmbH.
RSYSLOG_KSI_BIN="http://verify.guardtime.com/ksi-publications.bin"

echo "[ksi-extract-verify-short.sh]: testing rsgtutil extract function - short options"
. $srcdir/diag.sh init

echo "running rsgtutil extract command"
../tools/rsgtutil -s -x 1,3,5 -o $srcdir/ksi-export.log -P http://verify.guardtime.com/ksi-publications.bin $srcdir/testsuites/ksi-sample.log

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "[ksi-extract-verify-short.sh]: rsgtutil extract failed with error: " $RSYSLOGD_EXIT
	exit 1;
fi

echo "running rsgtutil verify command"
../tools/rsgtutil -t -s -P http://verify.guardtime.com/ksi-publications.bin $srcdir/ksi-export.log

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "[ksi-extract-verify-short.sh]: rsgtutil verify failed with error: " $RSYSLOGD_EXIT
	exit 1;
fi

# Cleanup temp files
rm -f $srcdir/ksi-export.*

echo SUCCESS: rsgtutil extract function - short options