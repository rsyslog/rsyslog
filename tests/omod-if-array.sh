#!/bin/bash
echo \[omod-if-array.sh\]: test omod-if-array via udp
echo NOTE: the interface checked with this test is currently NOT
echo supported. We may support it again in the future. So for now\,
echo we just skip this test and do not remove it.
exit 77
$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason

./nettester -tomod-if-array -iudp -p4711
if [ "$?" -ne "0" ]; then
  exit 1
fi

echo test omod-if-array via tcp
./nettester -tomod-if-array -itcp
if [ "$?" -ne "0" ]; then
  exit 1
fi

