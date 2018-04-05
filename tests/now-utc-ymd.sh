#!/bin/bash
# test many concurrent tcp connections
# addd 2016-02-23 by RGerhards, released under ASL 2.0
# requires faketime
. $srcdir/diag.sh init
echo \[now-utc-ymd\]: test \$year-utc, \$month-utc, \$day-utc

. $srcdir/faketime_common.sh

export TZ=TEST-02:00

. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$InputTCPServerRun 13514

template(name="outfmt" type="string"
	 string="%$year%-%$month%-%$day%,%$year-utc%-%$month-utc%-%$day-utc%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="rsyslog.out.log")
'
FAKETIME='2016-01-01 01:00:00' $srcdir/diag.sh startup
# what we send actually is irrelevant, as we just use system properties.
# but we need to send one message in order to gain output!
. $srcdir/diag.sh tcpflood -m1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo "2016-01-01,2015-12-31" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  exit 1
fi;


. $srcdir/diag.sh exit
