#!/bin/bash
# add 2018-04-13 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/impstats/.libs/impstats" interval="300"
	resetCounters="on" format="cee" ruleset="fooruleset" log.syslog="on")
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")

action(name="fooname" type="mmjsonparse" container="foobar")

action(type="omfile" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "mmjsonparse: invalid container name 'foobar', name must start with" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo "FAIL: expected error message not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        . $srcdir/diag.sh error-exit 1
fi

grep "impstats: ruleset 'fooruleset' not found - using default ruleset instead" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo "FAIL: expected error message not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        . $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
