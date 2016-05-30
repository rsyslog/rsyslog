#!/bin/bash
# rsgtutil utility test 
#	Extract lines from sample logdata and verifies against public 
#	ksi verification repository. 
#
# This file is part of the rsyslog project, released under ASL 2.0
#
# Copyright 2016 Rainer Gerhards and Adiscon GmbH.
RSYSLOG_KSI_BIN="http://verify.guardtime.com/ksi-publications.bin"
RSYSLOG_KSI_DEBUG="-s"
RSYSLOG_KSI_LOG="ksi-sample.log"

echo "[ksi-extract-verify-short.sh]: testing rsgtutil extract function - short options"
. $srcdir/diag.sh init

echo "running rsgtutil extract command"
../tools/rsgtutil $RSYSLOG_KSI_DEBUG -x 4,8,21 -o $srcdir/ksi-export.log -P http://verify.guardtime.com/ksi-publications.bin $srcdir/testsuites/$RSYSLOG_KSI_LOG

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then	# EX_OK
	if [ "$RSYSLOGD_EXIT" -eq "69" ]; then	# EX_UNAVAILABLE
		echo "[ksi-extract-verify-short.sh]: rsgtutil extract failed with service unavailable (does not generate an error)"
		exit 77;
	else
		echo "[ksi-extract-verify-short.sh]: rsgtutil extract failed with error: " $RSYSLOGD_EXIT
		exit 1;
	fi 
fi

if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "[.sh]: rsgtutil extract failed with error: " $RSYSLOGD_EXIT
	exit 1;
fi

echo "running rsgtutil verify command"../tools/rsgtutil -t -P http://verify.guardtime.com/ksi-publications.bin $srcdir/ksi-export.log

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then	# EX_OK
	if [ "$RSYSLOGD_EXIT" -eq "69" ]; then	# EX_UNAVAILABLE
		echo "[ksi-extract-verify-short.sh]: rsgtutil verify failed with service unavailable (does not generate an error)"
		exit 77;
	else
		echo "[ksi-extract-verify-short.sh]: rsgtutil verify failed with error: " $RSYSLOGD_EXIT
		exit 1;
	fi 
fi

# Cleanup temp files
rm -f $srcdir/ksi-export.*

echo SUCCESS: rsgtutil extract function - short options