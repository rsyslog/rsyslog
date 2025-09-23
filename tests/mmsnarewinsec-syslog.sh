#!/bin/bash
# Validate mmsnarewinsec when receiving raw syslog messages over TCP.
unset RSYSLOG_DYNNAME
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmsnarewinsec/.libs/mmsnarewinsec")

template(name="outfmt" type="list") {
    property(name="$!win!Event!EventID")
    constant(value=",")
    property(name="$!win!Event!Channel")
    constant(value=",")
    property(name="$!win!Event!EventType")
    constant(value=",")
    property(name="$!win!Event!CategoryText")
    constant(value=",")
    property(name="$!win!Event!Computer")
    constant(value="\n")
}

ruleset(name="winsec") {
    action(type="mmsnarewinsec")
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="winsec")
'

startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
cat "$srcdir/testsuites/mmsnarewinsec/sample-windows2022-security.data" \
    "$srcdir/testsuites/mmsnarewinsec/sample-windows2025-security.data" \
    > ${RSYSLOG_DYNNAME}.payload
tcpflood -m 1 -I ${RSYSLOG_DYNNAME}.payload
rm -f ${RSYSLOG_DYNNAME}.payload

shutdown_when_empty
wait_shutdown

content_check '4608,Security,Success Audit,Security State Change,WIN-5SB1I3G0V7U' $RSYSLOG_OUT_LOG
content_check '4616,Security,Success Audit,Security State Change,WIN-5SB1I3G0V7U' $RSYSLOG_OUT_LOG

exit_test
