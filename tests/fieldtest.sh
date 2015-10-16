#!/bin/bash
echo \[fieldtest.sh\]: test fieldtest via udp
. $srcdir/diag.sh init
$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason
. $srcdir/diag.sh generate-HOSTNAME

./nettester -tfield1 -iudp
if [ "$?" -ne "0" ]; then
  exit 1
fi

echo test fieldtest via tcp
./nettester -tfield1 -itcp
if [ "$?" -ne "0" ]; then
  exit 1
fi
. $srcdir/diag.sh exit
