#!/bin/bash
# Uses GeoIP2-City-Test.mmdb from https://github.com/maxmind/MaxMind-DB.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%$!iplocation%\n")

module(load="../plugins/mmdblookup/.libs/mmdblookup")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
	action(type="mmnormalize" rulebase="'$srcdir'/mmdb.rb")
	action(type="mmdblookup" mmdbfile="'$srcdir'/GeoIP2-City-Test.mmdb" key="$!ip"
		fields=["!continent!code","!city!names!en","!location!latitude","!location!longitude","!location!accuracy_radius"] )
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}'
startup
tcpflood -m 1 -j "81.2.69.160\ "
shutdown_when_empty
wait_shutdown
# continent!code is a deep leaf string
content_check '"code": "EU"'
# city!names!en is a 3-level deep leaf string
content_check '"en": "London"'
# location!latitude is a double
content_check '"latitude": 51.5142'
# location!accuracy_radius is a uint16
content_check '"accuracy_radius": 100'
exit_test
