#!/bin/bash
# test many concurrent tcp connections
# addd 2016-02-23 by RGerhards, released under ASL 2.0
# requires faketime
. $srcdir/diag.sh init
echo \[now-utc-ymd\]: test \$year-utc, \$month-utc, \$day-utc

. $srcdir/faketime_common.sh

export TZ=TEST-02:00

generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$InputTCPServerRun 13514

template(name="outfmt" type="string"
	 string="%$year%-%$month%-%$day%,%$year-utc%-%$month-utc%-%$day-utc%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
FAKETIME='2016-01-01 01:00:00' startup
# what we send actually is irrelevant, as we just use system properties.
# but we need to send one message in order to gain output!
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "2016-01-01,2015-12-31" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  exit 1
fi;


exit_test
