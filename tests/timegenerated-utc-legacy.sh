#!/bin/bash
# addd 2016-03-22 by RGerhards, released under ASL 2.0

# NOTE: faketime does NOT properly support subseconds,
# so we must ensure we do not use them. Actually, what we
# see is uninitialized data value in tv_usec, which goes
# away as soon as we do not run under faketime control.
# FOR THE SAME REASON, there is NO VALGRIND EQUIVALENT
# of this test, as valgrind would abort with reports
# of faketime.
. $srcdir/diag.sh init
. $srcdir/faketime_common.sh

export TZ=TEST+02:00

. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$InputTCPServerRun 13514

template(name="outfmt" type="string"
	 string="%timegenerated:::date-utc%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="rsyslog.out.log")
'

echo "***SUBTEST: check 2016-03-01"
rm -f rsyslog.out.log	# do cleanup of previous subtest
FAKETIME='2016-03-01 12:00:00' $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo "Mar  1 14:00:00" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  exit 1
fi;

. $srcdir/diag.sh exit
