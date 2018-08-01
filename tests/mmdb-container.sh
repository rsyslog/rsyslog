#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0 
. $srcdir/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%$!mmdb_root%\n")

module(load="../plugins/mmdblookup/.libs/mmdblookup" container="!mmdb_root")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514" ruleset="testing")

ruleset(name="testing") {
	action(type="mmnormalize" rulebase=`echo $srcdir/mmdb.rb`)
	action(type="mmdblookup" mmdbfile=`echo $srcdir/test.mmdb` key="$!ip" fields="city" )
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}'
startup
. $srcdir/diag.sh tcpflood -m 1 -j "202.106.0.20\ "
shutdown_when_empty
wait_shutdown
. $srcdir/diag.sh content-check '{ "city": "Beijing" }'
exit_test
