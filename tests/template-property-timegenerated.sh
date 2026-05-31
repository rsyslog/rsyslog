#!/bin/bash
# This focused template/property test uses faketime to make generated-time
# MsgGetProp formatting deterministic. It covers UTC conversion and generated
# timestamp date-part variants without depending on wall-clock time.
. ${srcdir:=.}/diag.sh init
. $srcdir/faketime_common.sh

export TZ=TEST+02:00

generate_conf
add_conf '
template(name="outfmt" type="list") {
	constant(value="generated_utc=")
	property(name="timegenerated" date.inUTC="on")
	constant(value="\ngenerated_parts=")
	property(name="timegenerated" dateformat="year" date.inUTC="on")
	constant(value="-")
	property(name="timegenerated" dateformat="month" date.inUTC="on")
	constant(value="-")
	property(name="timegenerated" dateformat="day" date.inUTC="on")
	constant(value="T")
	property(name="timegenerated" dateformat="hour" date.inUTC="on")
	constant(value=":")
	property(name="timegenerated" dateformat="minute" date.inUTC="on")
	constant(value=":")
	property(name="timegenerated" dateformat="second" date.inUTC="on")
	constant(value="\n")
}

local4.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

export FAKETIME='2016-03-01 12:34:56'
startup
injectmsg_literal '<167>1 2003-08-24T05:14:15.000003-07:00 host app proc msgid - trigger'
shutdown_when_empty
wait_shutdown

export EXPECTED='generated_utc=Mar  1 14:34:56
generated_parts=2016-03-01T14:34:56'
cmp_exact
exit_test
