#!/bin/bash
# add 2016-11-22 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omstdout/.libs/omstdout")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="-%msg%-\n")
action(type="omstdout" template="outfmt")

'
. $srcdir/diag.sh startup > rsyslog.out.log
. $srcdir/diag.sh tcpflood -m1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdowna

grep "msgnum:00000000:" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected message not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        . $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
