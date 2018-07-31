#!/bin/bash
# add 2016-11-22 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omstdout/.libs/omstdout")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="-%msg%-\n")
action(type="omstdout" template="outfmt")

'
startup > rsyslog.out.log
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdowna

grep "msgnum:00000000:" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected message not found. rsyslog.out.log is:"
        cat $RSYSLOG_OUT_LOG
        error_exit 1
fi

exit_test
