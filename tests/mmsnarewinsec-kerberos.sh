#!/bin/bash
# Kerberos-focused test for mmsnarewinsec: validate client address/port and certificate info
unset RSYSLOG_DYNNAME
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmsnarewinsec/.libs/mmsnarewinsec")

template(name="kjson" type="list" option.jsonf="on") {
  property(outname="EventID" name="$!win!Event!EventID" format="jsonf")
  property(outname="ClientAddress" name="$!win!Network!ClientAddress" format="jsonf")
  property(outname="ClientPort" name="$!win!Network!ClientPort" format="jsonf")
  property(outname="TicketOptions" name="$!win!Kerberos!TicketOptions" format="jsonf")
  property(outname="ResultCode" name="$!win!Kerberos!ResultCode" format="jsonf")
  property(outname="TicketEncryptionType" name="$!win!Kerberos!TicketEncryptionType" format="jsonf")
  property(outname="PreAuthenticationType" name="$!win!Kerberos!PreAuthenticationType" format="jsonf")
  property(outname="CertificateInfo" name="$!win!Kerberos!CertificateInfo" format="jsonf")
}

ruleset(name="winsec") {
  action(type="mmsnarewinsec")
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="kjson")
}

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="winsec")
'

startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
# Use sample events that include Kerberos service ticket (4769)
tcpflood -m 1 -I ${srcdir}/testsuites/mmsnarewinsec/sample-events.data

shutdown_when_empty
wait_shutdown

# Assert we saw Kerberos service ticket event and extracted client addr/port and additional info
content_check '{"eventid":"4769"' $RSYSLOG_OUT_LOG
content_check '"clientaddress":"172.16.14.21"' $RSYSLOG_OUT_LOG
content_check '"clientport":"55231"' $RSYSLOG_OUT_LOG
content_check '"ticketoptions":"0x60810010"' $RSYSLOG_OUT_LOG
content_check '"resultcode":"0x0"' $RSYSLOG_OUT_LOG
content_check '"ticketencryptiontype":"0x12"' $RSYSLOG_OUT_LOG
content_check '"preauthenticationtype":"15"' $RSYSLOG_OUT_LOG

exit_test

