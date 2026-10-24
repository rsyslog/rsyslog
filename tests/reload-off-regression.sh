#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run unchanged against the feature and pre-feature baseline. With reload mode
# absent, a failed second TCP bind must leave the healthy listener operational;
# legacy actions, ResetConfigVariables, unknown property rejection and optional
# includes retain startup behavior. HUP reopens legacy output with no reload
# diagnostic. Another testbench instance owns the occupied dynamic port, so
# there is no port-selection race or timing assumption. Regular diagnostics
# and traffic use omfile; bounded startup and shutdown use the harness guard.
# Legacy reset clears builtin parser/template registries; this fixture restores
# parser instances and a diagnostic template explicitly after the reset while
# testing that earlier modern global/main_queue values remain effective.
# Declare the failed input before the healthy one: imtcp prepends servers, so
# the healthy server must own the main input loop for cooperative shutdown on
# the baseline. Both listeners and all failure/traffic oracles remain present.
# Preserve the normal input shutdown budget through the modern setting before
# reset. The mandatory harness guard and proper termination oracle stay active.
# Unknown-property rejection is checked in a separate -N1 parser invocation: its
# invalid template has no usable output route, so parser stderr is its oracle.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf 'module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
action(type="omfile" file="'$RSYSLOG_DYNNAME'.holder")'
startup
occupied_port=$TCPFLOOD_PORT
mkdir "$RSYSLOG_DYNNAME.work"
generate_conf 2
# Reset reuses action numbering. Keep the secondary preamble free of actions;
# all fixture output actions are deliberately configured after the reset.
sed '/^if \$syslogtag contains "rsyslogd" then action/d' "${TESTCONF_NM}2.conf" > "$RSYSLOG_DYNNAME.preamble2" || error_exit 1
mv "$RSYSLOG_DYNNAME.preamble2" "${TESTCONF_NM}2.conf" || error_exit 1
input_shutdown_timeout=${RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT:-60000}
add_conf 'global(processInternalMessages="on" internalmsg.severity="info" inputs.timeout.shutdown="'"$input_shutdown_timeout"'" workDirectory="'$PWD/$RSYSLOG_DYNNAME'.work")
main_queue(queue.type="Disk" queue.filename="off-preserved-mainq" queue.checkpointInterval="1")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="'$occupied_port'")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.healthy-port")
include(file="'$RSYSLOG_DYNNAME'.does-not-exist" mode="optional")
$ResetConfigVariables
# Restore registries reset by the ordinary legacy loader. The modern global
# and main_queue objects above must still take effect when startup finalizes.
parser(name="afterReset3164" type="pmrfc3164")
$RulesetParser afterReset3164
$template legacyfmt,"%msg%\n"
$ActionFileDefaultTemplate legacyfmt
:msg, contains, "off-marker" '$PWD/$RSYSLOG_OUT_LOG'
template(name="afterReset" type="string" string="%msg%\n")
*.* action(type="omfile" file="'$RSYSLOG_DYNNAME'.diagnostics" template="afterReset")
' 2
# Preserve the usable active configuration while separately checking that the
# ordinary parser rejects an unknown property exactly as before the feature.
cp "${TESTCONF_NM}2.conf" "$RSYSLOG_DYNNAME.bad-property.conf"
printf '$template badProperty,"%%unknown-off-regression-property%%\\n"\n' >> "$RSYSLOG_DYNNAME.bad-property.conf"
../tools/rsyslogd -N1 -f"$RSYSLOG_DYNNAME.bad-property.conf" -M"$RSYSLOG_MODDIR" >"$RSYSLOG_DYNNAME.bad-property.parser" 2>&1
[ "$?" = 1 ] || error_exit 1
content_check "invalid property 'unknown-off-regression-property'" "$RSYSLOG_DYNNAME.bad-property.parser"
startup 2
# The imdiag port can precede the daemon PID file; HUP needs a populated PID.
wait_rsyslog_instance_pid 2
wait_file_exists "$RSYSLOG_DYNNAME.healthy-port"
healthy_port=$(cat "$RSYSLOG_DYNNAME.healthy-port")
tcpflood -p"$healthy_port" -m1 -M'<167>Mar  1 01:00:00 localhost test: off-marker-before'
wait_content off-marker-before
# A real disk queue segment proves both the modern main_queue setting and its
# global workDirectory survived the following legacy ResetConfigVariables.
compgen -G "$RSYSLOG_DYNNAME.work/off-preserved-mainq.*" > "$RSYSLOG_DYNNAME.queue-files" || error_exit 1
mv "$RSYSLOG_OUT_LOG" "$RSYSLOG_DYNNAME.rotated"
kill -HUP "$(cat "${RSYSLOG_PIDBASE}2.pid")"
echo AwaitHUPComplete | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT2" || error_exit $?
tcpflood -p"$healthy_port" -m1 -M'<167>Mar  1 01:00:00 localhost test: off-marker-after'
wait_content off-marker-after
shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown
content_check off-marker-before "$RSYSLOG_DYNNAME.rotated"
content_check off-marker-after
content_check 'Could not create tcp listener for port' "$RSYSLOG_DYNNAME.diagnostics"
check_not_present reload_validate "$RSYSLOG_DYNNAME.diagnostics"
exit_test
