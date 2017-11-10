#!/bin/bash 
# This file is part of the rsyslog project, released under ASL 2.0 

# we libmaxmindb, in packaged versions, has a small cosmetic memory leak,
# thus we need a supressions file:
export RS_TESTBENCH_VALGRIND_EXTRA_OPTS="$RS_TESTBENCH_VALGRIND_EXTRA_OPTS --suppressions=libmaxmindb.supp"

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="string" string="%$!iplocation%\n")

module(load="../plugins/mmdblookup/.libs/mmdblookup")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514" ruleset="testing")

ruleset(name="testing") {
set $!a = "a";
	action(type="mmnormalize" rulebase="./mmdb.rb")
	action(type="mmdblookup" mmdbfile="./test.mmdb" key="$!ip" fields="city" )
	action(type="omfile" file="./rsyslog.out.log" template="outfmt")
}'
. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh tcpflood -m 100 -j "202.106.0.20\ "
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-check '{ "city": "Beijing" }'
. $srcdir/diag.sh exit 
