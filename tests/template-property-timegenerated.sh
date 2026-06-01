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
	constant(value="\ngenerated_local=")
	property(name="timegenerated")
	constant(value="\ngenerated_local_formats=")
	property(name="timegenerated" dateformat="mysql")
	constant(value="/")
	property(name="timegenerated" dateformat="pgsql")
	constant(value="/")
	property(name="timegenerated" dateformat="rfc3164")
	constant(value="/")
	property(name="timegenerated" dateformat="rfc3339")
	constant(value="/")
	property(name="timegenerated" dateformat="unixtimestamp")
	constant(value="/")
	property(name="timegenerated" dateformat="wdayname")
	constant(value="/")
	property(name="timegenerated" dateformat="wday")
	constant(value="/")
	property(name="timegenerated" dateformat="ordinal")
	constant(value="/")
	property(name="timegenerated" dateformat="week")
	constant(value="\ngenerated_utc_formats=")
	property(name="timegenerated" dateformat="mysql" date.inUTC="on")
	constant(value="/")
	property(name="timegenerated" dateformat="pgsql" date.inUTC="on")
	constant(value="/")
	property(name="timegenerated" dateformat="rfc3164" date.inUTC="on")
	constant(value="/")
	property(name="timegenerated" dateformat="rfc3339" date.inUTC="on")
	constant(value="/")
	property(name="timegenerated" dateformat="unixtimestamp" date.inUTC="on")
	constant(value="/")
	property(name="timegenerated" dateformat="wdayname" date.inUTC="on")
	constant(value="/")
	property(name="timegenerated" dateformat="wday" date.inUTC="on")
	constant(value="/")
	property(name="timegenerated" dateformat="ordinal" date.inUTC="on")
	constant(value="/")
	property(name="timegenerated" dateformat="week" date.inUTC="on")
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
generated_local=Mar  1 12:34:56
generated_local_formats=20160301123456/2016-03-01 12:34:56/Mar  1 12:34:56/2016-03-01T12:34:56.000000-02:00/1456842896/Tue/2/061/10
generated_utc_formats=20160301143456/2016-03-01 14:34:56/Mar  1 14:34:56/2016-03-01T14:34:56.000000+00:00/1456842896/Tue/2/061/10
generated_parts=2016-03-01T14:34:56'
cmp_exact
exit_test
