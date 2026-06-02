#!/bin/bash
# add 2018-06-25 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%timestamp:::date-rfc3339%\n")

:syslogtag, contains, "su" action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`
				  template="outfmt")

'
startup
injectmsg_literal "<34>1 2003-11-11T22:14:15.003Z mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2003-01-11T22:14:15.003Z mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2003-11-01T22:04:15.003Z mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2003-11-11T02:14:15.003Z mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2003-11-11T22:04:05.003Z mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2003-11-11T22:04:05.003+02:00 mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2003-11-11T22:04:05.003+01:30 mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2003-11-11T22:04:05.123456+01:30 mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2026-04-27T15:46:41.0300000Z mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2026-04-27T15:46:41.03000000Z mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2026-04-27T15:46:41.000006930Z mymachine.example.com su - ID47 - MSG"
injectmsg_literal "<34>1 2026-04-27T15:46:41.000022054Z mymachine.example.com su - ID47 - MSG"
shutdown_when_empty
wait_shutdown

export EXPECTED='2003-11-11T22:14:15.003Z
2003-01-11T22:14:15.003Z
2003-11-01T22:04:15.003Z
2003-11-11T02:14:15.003Z
2003-11-11T22:04:05.003Z
2003-11-11T22:04:05.003+02:00
2003-11-11T22:04:05.003+01:30
2003-11-11T22:04:05.123456+01:30
2026-04-27T15:46:41.030000Z
2026-04-27T15:46:41.030000Z
2026-04-27T15:46:41.000006Z
2026-04-27T15:46:41.000022Z'
cmp_exact
exit_test
