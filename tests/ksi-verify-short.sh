#!/bin/bash
# rsgtutil utility test 
#	Verifies sample logdata against public ksi verification repository. 
#
# This file is part of the rsyslog project, released under ASL 2.0
#
# Copyright 2016 Rainer Gerhards and Adiscon GmbH.
RSYSLOG_KSI_BIN="http://verify.guardtime.com/ksi-publications.bin"

echo \[ksi-verify-short.sh\]: testing rsgtutil verify function - short options
. $srcdir/diag.sh init

echo "running rsgtutil command with short options"
../tools/rsgtutil -t -s -P $RSYSLOG_KSI_BIN $srcdir/testsuites/ksi-sample.log #> rsgtutil.out.log 2>&1

RSYSLOGD_EXIT=$?
if [ "$RSYSLOGD_EXIT" -ne "0" ]; then
	echo "rsgtutil returned error: " $RSYSLOGD_EXIT
	exit 1;
fi

# Cleanup temp files
rm -f rsgtutil.out*.log 

echo SUCCESS: rsgtutil verify function - short options