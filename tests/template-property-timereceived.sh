#!/bin/bash
# This focused template/property test proves that timereceived is accepted as a
# message property alias for timegenerated and formats the same received-time
# value under deterministic faketime.
. ${srcdir:=.}/diag.sh init
. $srcdir/faketime_common.sh

export TZ=TEST+02:00

generate_conf
add_conf '
template(name="outfmt" type="list") {
	constant(value="generated=")
	property(name="timegenerated" dateformat="rfc3339" date.inUTC="on")
	constant(value="\nreceived=")
	property(name="timereceived" dateformat="rfc3339" date.inUTC="on")
	constant(value="\nlegacy=")
	property(name="timegenerated" dateformat="unixtimestamp")
	constant(value="\nalias=")
	property(name="timereceived" dateformat="unixtimestamp")
	constant(value="\n")
}

local4.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

export FAKETIME='2016-03-01 12:34:56'
startup
injectmsg_literal '<167>1 2003-08-24T05:14:15.000003-07:00 host app proc msgid - trigger'
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 4
shutdown_when_empty
wait_shutdown

export EXPECTED='generated=2016-03-01T14:34:56.000000+00:00
received=2016-03-01T14:34:56.000000+00:00
legacy=1456842896
alias=1456842896'
cmp_exact
exit_test
