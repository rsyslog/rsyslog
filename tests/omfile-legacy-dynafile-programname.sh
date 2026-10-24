#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# A legacy omfile dynafile must resolve RFC5424 APP-NAME as programname on its
# first action with default-disabled input rate limiting. Modern omfile and a legacy
# action preceded by DebugFormat control for the same input fields. A separate
# rate-limited input must preserve the first accepted filename and identify
# the second APP-NAME in the first-drop diagnostic. A malformed first drop
# must retain the drop decision and use a fallback label. Each
# file is a direct oracle; a later property read cannot repair its filename.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$template dynfile,"'${PWD}'/'${RSYSLOG_DYNNAME}'.legacy/%HOSTNAME%/%PROGRAMNAME%.log"
$template primedfile,"'${PWD}'/'${RSYSLOG_DYNNAME}'.primed/%HOSTNAME%/%PROGRAMNAME%.log"
$template limitedfile,"'${PWD}'/'${RSYSLOG_DYNNAME}'.limited/%HOSTNAME%/%PROGRAMNAME%.log"
template(name="modernfile" type="string" string="'${PWD}'/'${RSYSLOG_DYNNAME}'.modern/%HOSTNAME%/%PROGRAMNAME%.log")
$RuleSet fromnet
*.* ?dynfile;RSYSLOG_SyslogProtocol23Format
*.* '${PWD}'/'${RSYSLOG_OUT_LOG}';RSYSLOG_DebugFormat
$RuleSet primed
*.* '${PWD}'/'${RSYSLOG_DYNNAME}'.primed-fields.log;RSYSLOG_DebugFormat
*.* ?primedfile;RSYSLOG_SyslogProtocol23Format
$RuleSet limited
*.* ?limitedfile;RSYSLOG_SyslogProtocol23Format
ruleset(name="modern") {
    action(type="omfile" dynaFile="modernfile" template="RSYSLOG_SyslogProtocol23Format")
}
ruleset(name="limitedbad" parser="rsyslog.rfc5424") {
    action(type="omfile" file="'${RSYSLOG_DYNNAME}'.limitedbad.log")
}
$InputTCPServerBindRuleset fromnet
$InputTCPServerListenPortFile '${RSYSLOG_DYNNAME}'.tcpflood_port
$InputTCPServerRun 0
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.modern.port" ruleset="modern")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.primed.port" ruleset="primed")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.limited.port" ruleset="limited" ratelimit.interval="600" ratelimit.burst="1")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.limitedbad.port" ruleset="limitedbad" ratelimit.interval="600" ratelimit.burst="1")
'

startup
tcpflood -m1 -M '"<14>1 2026-01-01T00:00:00Z myhost myprogram - - - hello world"'
tcpflood -p "$(cat "${RSYSLOG_DYNNAME}.modern.port")" -m1 -M '"<14>1 2026-01-01T00:00:00Z myhost modernapp - - - modern control"'
tcpflood -p "$(cat "${RSYSLOG_DYNNAME}.primed.port")" -m1 -M '"<14>1 2026-01-01T00:00:00Z myhost primedapp - - - primed control"'
# A single connection preserves input order for each burst=1 drop oracle.
printf '%s\n' \
    '<14>1 2026-01-01T00:00:00Z myhost limitedapp - - - first limited' \
    '<14>1 2026-01-01T00:00:00Z myhost droppedapp - - - second limited' > "${RSYSLOG_DYNNAME}.limited.input"
tcpflood -p "$(cat "${RSYSLOG_DYNNAME}.limited.port")" -I "${RSYSLOG_DYNNAME}.limited.input"
printf '%s\n' \
    '<14>1 2026-01-01T00:00:00Z myhost goodapp - - - first malformed-control' \
    'this is not syslog' > "${RSYSLOG_DYNNAME}.limitedbad.input"
tcpflood -p "$(cat "${RSYSLOG_DYNNAME}.limitedbad.port")" -I "${RSYSLOG_DYNNAME}.limitedbad.input"
shutdown_when_empty
wait_shutdown

content_check "programname: 'myprogram'" "$RSYSLOG_OUT_LOG"
content_check 'modern control' "${RSYSLOG_DYNNAME}.modern/myhost/modernapp.log"
content_check 'primed control' "${RSYSLOG_DYNNAME}.primed/myhost/primedapp.log"
content_check 'first limited' "${RSYSLOG_DYNNAME}.limited/myhost/limitedapp.log"
if [ -e "${RSYSLOG_DYNNAME}.limited/myhost/droppedapp.log" ]; then
    echo 'FAIL: the second message escaped the configured input rate limit'
    error_exit 1
fi
content_check 'from <myhost:droppedapp>: begin to drop messages due to rate-limiting' "${RSYSLOG_DYNNAME}.started"
content_check 'first malformed-control' "${RSYSLOG_DYNNAME}.limitedbad.log"
content_check 'from <unparsed>: begin to drop messages due to rate-limiting' "${RSYSLOG_DYNNAME}.started"
if [ -e "${RSYSLOG_DYNNAME}.legacy/myhost/.log" ]; then
    echo 'FAIL: first dynafile lookup used empty programname'
    error_exit 1
fi
content_check 'hello world' "${RSYSLOG_DYNNAME}.legacy/myhost/myprogram.log"
exit_test
