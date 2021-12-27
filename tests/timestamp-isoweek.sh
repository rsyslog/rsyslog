#!/bin/bash
# add 2021-12-27 by Mattia Barbon, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%timestamp:::date-iso-week-year%/%timestamp:::date-iso-week%\n")

:syslogtag, contains, "su" action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`
				   template="outfmt")


'
startup
injectmsg_literal "<34>1 1971-01-01T12:34:56.003Z mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2021-12-02T12:34:56.123456Z mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2099-12-31T12:34:56Z mymachine.example.com su - ID47 - MSG"
shutdown_when_empty
wait_shutdown

export EXPECTED='1970/53
2021/48
2099/53'
cmp_exact
exit_test
