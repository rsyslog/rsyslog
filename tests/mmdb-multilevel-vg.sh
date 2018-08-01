#!/bin/bash 
# This file is part of the rsyslog project, released under ASL 2.0 

# we libmaxmindb, in packaged versions, has a small cosmetic memory leak,
# thus we need a supressions file:
export RS_TESTBENCH_VALGRIND_EXTRA_OPTS="$RS_TESTBENCH_VALGRIND_EXTRA_OPTS --suppressions=$srcdir/libmaxmindb.supp"

. $srcdir/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%$!iplocation%\n")

module(load="../plugins/mmdblookup/.libs/mmdblookup")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514" ruleset="testing")

ruleset(name="testing") {
	action(type="mmnormalize" rulebase=`echo $srcdir/mmdb.rb`)
	# Uncomment this action when using the real GeoLite2 city database;
	# we have not included it into the testbench for licensing concerns.
	# action(type="mmdblookup" mmdbfile="/home/USR/GeoLite2-City_20170502/GeoLite2-City.mmdb" key="$!ip" fields=":city:!city!names!en" )
	action(type="mmdblookup" mmdbfile=`echo $srcdir/test.mmdb` key="$!ip" fields=":city_name:city" )
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}'
startup_vg
. $srcdir/diag.sh tcpflood -m 100 -j "202.106.0.20\ "
shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-check '{ "city_name": "Beijing" }'
exit_test 
