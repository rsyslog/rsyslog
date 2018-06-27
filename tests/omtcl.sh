#!/bin/bash
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
$ModLoad ../contrib/omtcl/.libs/omtcl
$template tcldict, "message \"%msg:::json%\" fromhost \"%HOSTNAME:::json%\" facility \"%syslogfacility-text%\" priority \"%syslogpriority-text%\" timereported \"%timereported:::date-rfc3339%\" timegenerated \"%timegenerated:::date-rfc3339%\" raw \"%rawmsg:::json%\" tag \"%syslogtag:::json%\""
'
. $srcdir/diag.sh add-conf "*.* :omtcl:$srcdir/omtcl.tcl,doAction;tcldict
"
. $srcdir/diag.sh startup
echo 'injectmsg litteral <167>Mar  1 01:00:00 172.20.245.8 tag hello world' | \
	./diagtalker || . $srcdir/diag.sh error-exit $?
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh content-check 'HELLO WORLD'
cat rsyslog.out.log
. $srcdir/diag.sh exit
