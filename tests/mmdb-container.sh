#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0 
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="string" string="%$!mmdb_root%\n")

module(load="../plugins/mmdblookup/.libs/mmdblookup" container="!mmdb_root")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514" ruleset="testing")

ruleset(name="testing") {
	action(type="mmnormalize" rulebase="./mmdb.rb")
	action(type="mmdblookup" mmdbfile="./test.mmdb" key="$!ip" fields="city" )
	action(type="omfile" file="./rsyslog.out.log" template="outfmt")
}'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m 1 -j "202.106.0.20\ "
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh content-check '{ "city": "Beijing" }'
. $srcdir/diag.sh exit
