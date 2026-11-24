#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# The best-effort survival budget must begin after the expected startup
# diagnostic, not at fork. An owned FIFO child becomes ready at injected clock
# tick two. All three survival ticks must follow its marker/acknowledgement;
# this is an ordering oracle with no wall-clock sleeps or scheduling threshold.
# A live never-ready child must instead hit the bounded readiness timeout and
# be stopped/reaped. The real stop helper remains active; its grace-period
# ticks are counted separately from survival ticks. No hosted cause is inferred.
. ${srcdir:=.}/diag.sh init
. ${srcdir:=.}/testsuites/imtcp-bind-failure-common.sh

export PROBE_DIR="$PWD/$RSYSLOG_DYNNAME.window"
mkdir -p "$PROBE_DIR"
cleanup_probe() {
	if [ -s "$PROBE_DIR/child.pid" ]; then
		kill "$(cat "$PROBE_DIR/child.pid")" 2>/dev/null || :
	fi
}
trap cleanup_probe EXIT
cat > "$PROBE_DIR/clock.sh" <<'CLOCK'
clock_tick() {
	local phase=$1 ticks=0 after=0 survival=0 reply
	[ ! -f "$PROBE_DIR/ticks" ] || read -r ticks < "$PROBE_DIR/ticks"
	[ ! -f "$PROBE_DIR/after" ] || read -r after < "$PROBE_DIR/after"
	[ ! -f "$PROBE_DIR/survival" ] || read -r survival < "$PROBE_DIR/survival"
	if [ "$phase" = survival ]; then survival=$((survival + 1)); fi
	if [ "$phase" = survival ] && [ -e "$PROBE_DIR/ready" ]; then
		after=$((after + 1))
	fi
	ticks=$((ticks + 1))
	printf '%s\n' "$ticks" > "$PROBE_DIR/ticks"
	printf '%s\n' "$after" > "$PROBE_DIR/after"
	printf '%s\n' "$survival" > "$PROBE_DIR/survival"
	if [ "$ticks" -eq 2 ] && [ "$PROBE_RELEASE" = yes ]; then
		exec 3<> "$PROBE_DIR/gate"
		exec 4<> "$PROBE_DIR/ack"
		printf 'release\n' >&3
		if ! IFS= read -r -t 5 reply <&4 || [ "$reply" != ready ]; then
			echo 'controlled child did not acknowledge readiness' >&2
			return 1
		fi
		exec 3>&- 4>&-
	fi
}
CLOCK
cat > "$PROBE_DIR/daemon" <<'CHILD'
#!/bin/bash
trap 'exit 0' TERM
printf '%s\n' "$$" > "$PROBE_DIR/child.pid"
read -r release < "$PROBE_DIR/gate"
printf '%s\n' "$PROBE_DIAGNOSTIC"
: > "$PROBE_DIR/ready"
printf 'ready\n' > "$PROBE_DIR/ack"
exec 3<> "$PROBE_DIR/hold"
read -r stop <&3
CHILD
cat > "$PROBE_DIR/msleep" <<'POLL'
#!/bin/bash
. "$PROBE_DIR/clock.sh"
clock_tick poll
POLL
chmod +x "$PROBE_DIR/daemon" "$PROBE_DIR/msleep"
. "$PROBE_DIR/clock.sh"
# Keep the extracted cleanup/kill/wait implementation active. Only its clock
# is virtual: distinguish its grace ticks from the survival loop's ticks.
sleep() {
	if [ "${FUNCNAME[1]}" = run_bounded ]; then
		clock_tick survival
	else
		clock_tick cleanup
	fi
}
export daemon="$PROBE_DIR/daemon"
export modules=''
export TESTTOOL_DIR="$PROBE_DIR"
export PROBE_DIAGNOSTIC="input module name 'imtcp-startup-policy-missing' is unknown"
export PROBE_RELEASE=yes
export TB_TIMEOUT_STARTSTOP=10
for name in gate ack hold; do mkfifo "$PROBE_DIR/$name"; done
run_bounded 3 "$PROBE_DIR/unused.conf" "$PROBE_DIR/ready.log" "" "$PROBE_DIAGNOSTIC"
status=$?
[ "$status" -eq 124 ] || error_exit 1 "ready child returned $status instead of survival status 124"
read -r after < "$PROBE_DIR/after"
[ "$after" -eq 3 ] || error_exit 1 "only $after survival ticks followed readiness, expected 3"
kill -0 "$(cat "$PROBE_DIR/child.pid")" 2>/dev/null && error_exit 1 "ready child was not reaped"

rm -f "$PROBE_DIR/ready" "$PROBE_DIR/ticks" "$PROBE_DIR/after" "$PROBE_DIR/survival" "$PROBE_DIR/child.pid"
PROBE_RELEASE=no
export TB_TIMEOUT_STARTSTOP=0
run_bounded 3 "$PROBE_DIR/unused.conf" "$PROBE_DIR/never.log" "" "$PROBE_DIAGNOSTIC"
status=$?
[ "$status" -eq 125 ] || error_exit 1 "never-ready child returned $status instead of readiness failure 125"
# The helper may stop the child before its script writes a pid; its own wait
# still reaps that child. If a pid was published, it must no longer be live.
if [ -s "$PROBE_DIR/child.pid" ]; then
	kill -0 "$(cat "$PROBE_DIR/child.pid")" 2>/dev/null && error_exit 1 "never-ready child was not reaped"
fi
survival=0
[ ! -f "$PROBE_DIR/survival" ] || read -r survival < "$PROBE_DIR/survival"
[ "$survival" -eq 0 ] || error_exit 1 "readiness timeout entered the survival clock"
trap - EXIT
rm -rf "$PROBE_DIR"
exit_test
