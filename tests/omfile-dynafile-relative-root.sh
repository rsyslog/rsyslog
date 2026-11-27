#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# A dynafile template without a fixed leading directory renders relative
# names. Without -C, rsyslogd works from the filesystem root, where such a
# name can select any location; the fallback guard must reject it, warn at
# startup and log the rejection only once. With -C the same name stays below
# the startup directory and must still be accepted.
# The testbench always uses -C, so this test starts rsyslogd itself. It needs
# no network input: the daemon's own startup message drives the action.
# imdiag is loaded with aborttimeout so the mandatory timeoutGuard aborts a
# wedged daemon from inside; the shell additionally bounds the shutdown wait
# and requires a clean exit (status 0) of each daemon run.
# RELROOT_YAML=YES defines the dynafile template and action through the YAML
# frontend (yaml-omfile-dynafile-relative-root.sh); the oracle is the same.
. ${srcdir:=.}/diag.sh init

export REL_DIR="tmp-${RSYSLOG_DYNNAME}"
export REL_NAME="${REL_DIR}/relative-root.log"
export ESCAPE_OUT="/${REL_NAME}"
export CWD_OUT="$(pwd)/${REL_NAME}"
export CONF="$(pwd)/${RSYSLOG_DYNNAME}.relroot.conf"
export YAML_CONF="$(pwd)/${RSYSLOG_DYNNAME}.relroot.yaml"
export CTL_OUT="$(pwd)/${RSYSLOG_DYNNAME}.relroot.ctl"
export PIDFILE="$(pwd)/${RSYSLOG_DYNNAME}.relroot.pid"
STARTUP_WARNING="omfile: dynafile template 'dynfile' has no fixed leading directory"
REJECTION="would resolve against the filesystem root"

cleanup_relroot() {
	rm -rf "/${REL_DIR}" "$(pwd)/${REL_DIR}" "$CONF" "$YAML_CONF" "$CTL_OUT" "$PIDFILE"
}
cleanup_relroot

# REL_DIR does not exist below /, and an unprivileged test cannot create it
# there; the escape check below therefore also detects a successful create.
cat > "$CONF" <<CONF_EOF
module(load="../plugins/imdiag/.libs/imdiag" serverrun="0"
       listenportfilename="$(pwd)/${RSYSLOG_DYNNAME}.relroot.imdiag.port"
       aborttimeout="${TB_TEST_MAX_RUNTIME}")
template(name="outfmt" type="string" string="%msg%\n")
template(name="ctlfmt" type="string" string="%msg%\n")
action(type="omfile" file="$CTL_OUT" template="ctlfmt")
CONF_EOF

if [ "${RELROOT_YAML:-}" = YES ]; then
	require_yaml_support
	cat > "$YAML_CONF" <<YAML_EOF
templates:
  - name: dynfile
    type: string
    string: "%\$!f%"

rulesets:
  - name: dyn
    actions:
      - type: omfile
        dynafile: dynfile
        template: outfmt
        createDirs: "on"
YAML_EOF
	cat >> "$CONF" <<CONF_EOF
include(file="$YAML_CONF")
if \$msg contains "start" and not (\$msg contains "blocked") then {
	set \$!f = "$REL_NAME";
	call dyn
}
CONF_EOF
else
	cat >> "$CONF" <<CONF_EOF
template(name="dynfile" type="string" string="%\$!f%")
if \$msg contains "start" and not (\$msg contains "blocked") then {
	set \$!f = "$REL_NAME";
	action(type="omfile" dynafile="dynfile" template="outfmt" createDirs="on")
}
CONF_EOF
fi

relroot_fail() {
	echo "FAIL: $1"
	cat "$CTL_OUT" 2>/dev/null
	cleanup_relroot
	error_exit 1
}

# $1: extra rsyslogd option ("" or "-C"); $2: shell condition that marks
# completion. Fails the test if rsyslogd exits early, the condition does not
# become true within 10 seconds, the daemon does not stop within 10 seconds
# after SIGTERM, or it exits with a nonzero status.
run_daemon() {
	local i done=0 rc
	rm -f "$CTL_OUT" "$PIDFILE"
	../tools/rsyslogd $1 -n -M"$RSYSLOG_MODDIR" -f"$CONF" -i"$PIDFILE" &
	local pid=$!
	for i in $(seq 1 100); do
		if eval "$2"; then
			done=1
			break
		fi
		if ! kill -0 "$pid" 2>/dev/null; then
			wait "$pid"
			relroot_fail "rsyslogd $1 exited early (status $?)"
		fi
		$TESTTOOL_DIR/msleep 100
	done
	kill "$pid"
	for i in $(seq 1 100); do
		kill -0 "$pid" 2>/dev/null || break
		$TESTTOOL_DIR/msleep 100
	done
	if kill -0 "$pid" 2>/dev/null; then
		kill -9 "$pid"
		wait "$pid"
		relroot_fail "rsyslogd $1 did not stop within 10 seconds after SIGTERM"
	fi
	wait "$pid"
	rc=$?
	if [ $done -ne 1 ]; then
		relroot_fail "rsyslogd $1 did not reach: $2"
	fi
	if [ $rc -ne 0 ]; then
		relroot_fail "rsyslogd $1 exited with status $rc"
	fi
}

run_daemon "" "grep -q '$REJECTION' \"\$CTL_OUT\" 2>/dev/null"
if [ -e "$ESCAPE_OUT" ]; then
	relroot_fail "relative dynafile name resolved against / and created: $ESCAPE_OUT"
fi
# The runtime rejection also names the template; match the startup warning by
# its own prefix so it is checked independently.
if ! grep -qF "$STARTUP_WARNING" "$CTL_OUT"; then
	relroot_fail "expected startup warning for template without fixed directory"
fi
if [ "$(grep -c "$REJECTION" "$CTL_OUT")" -ne 1 ]; then
	relroot_fail "relative-root rejection was not logged exactly once"
fi

run_daemon "-C" "[ -f \"\$CWD_OUT\" ]"
if [ -e "$ESCAPE_OUT" ]; then
	relroot_fail "relative dynafile name with -C escaped to: $ESCAPE_OUT"
fi

cleanup_relroot
exit_test
