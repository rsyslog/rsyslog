#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# A legacy omfile dynafile must resolve RFC5424 APP-NAME as programname on its
# first action. Modern omfile and a legacy action preceded by DebugFormat are
# controls: both resolve the same input fields. The first legacy filename is
# the regression oracle; a later property read cannot repair it.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$template dynfile,"'${PWD}'/'${RSYSLOG_DYNNAME}'.legacy/%HOSTNAME%/%PROGRAMNAME%.log"
$template primedfile,"'${PWD}'/'${RSYSLOG_DYNNAME}'.primed/%HOSTNAME%/%PROGRAMNAME%.log"
template(name="modernfile" type="string" string="'${PWD}'/'${RSYSLOG_DYNNAME}'.modern/%HOSTNAME%/%PROGRAMNAME%.log")
$RuleSet fromnet
*.* ?dynfile;RSYSLOG_SyslogProtocol23Format
*.* '${PWD}'/'${RSYSLOG_OUT_LOG}';RSYSLOG_DebugFormat
$RuleSet primed
*.* '${PWD}'/'${RSYSLOG_DYNNAME}'.primed-fields.log;RSYSLOG_DebugFormat
*.* ?primedfile;RSYSLOG_SyslogProtocol23Format
ruleset(name="modern") {
    action(type="omfile" dynaFile="modernfile" template="RSYSLOG_SyslogProtocol23Format")
}
$InputTCPServerBindRuleset fromnet
$InputTCPServerListenPortFile '${RSYSLOG_DYNNAME}'.tcpflood_port
$InputTCPServerRun 0
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.modern.port" ruleset="modern")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.primed.port" ruleset="primed")
'

startup
tcpflood -m1 -M '"<14>1 2026-01-01T00:00:00Z myhost myprogram - - - hello world"'
tcpflood -p "$(cat "${RSYSLOG_DYNNAME}.modern.port")" -m1 -M '"<14>1 2026-01-01T00:00:00Z myhost modernapp - - - modern control"'
tcpflood -p "$(cat "${RSYSLOG_DYNNAME}.primed.port")" -m1 -M '"<14>1 2026-01-01T00:00:00Z myhost primedapp - - - primed control"'
shutdown_when_empty
wait_shutdown

content_check "programname: 'myprogram'" "$RSYSLOG_OUT_LOG"
content_check 'modern control' "${RSYSLOG_DYNNAME}.modern/myhost/modernapp.log"
content_check 'primed control' "${RSYSLOG_DYNNAME}.primed/myhost/primedapp.log"
if [ -e "${RSYSLOG_DYNNAME}.legacy/myhost/.log" ]; then
    echo 'FAIL: first dynafile lookup used empty programname'
    error_exit 1
fi
content_check 'hello world' "${RSYSLOG_DYNNAME}.legacy/myhost/myprogram.log"
exit_test
