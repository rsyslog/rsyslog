#!/bin/bash
# add 2018-06-27 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%timestamp:::date-pgsql%\n")

:syslogtag, contains, "su" action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`
				   template="outfmt")


'
startup
injectmsg_literal "<34>1 2003-01-23T12:34:56.003Z mymachine.example.com su - ID47 - MSG\""
shutdown_when_empty
wait_shutdown

export EXPECTED='2003-01-23 12:34:56'
cmp_exact
exit_test
