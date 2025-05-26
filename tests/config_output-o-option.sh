#!/bin/bash
# check that the -o command line option works
# added 2019-04-26 by Rainer Gerhards; Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then {
	include(file="'${srcdir}'/testsuites/include-std-omfile-actio*.conf")
	continue
}
'
../tools/rsyslogd -N1 -f${TESTCONF_NM}.conf -o$RSYSLOG_DYNNAME.fullconf  -M../runtime/.libs:../.libs
content_check 'if $msg contains "msgnum:" then' $RSYSLOG_DYNNAME.fullconf
content_check 'action(type="omfile"' $RSYSLOG_DYNNAME.fullconf
content_check --regex "BEGIN CONFIG: .*include-std-omfile-action.conf" $RSYSLOG_DYNNAME.fullconf
exit_test
