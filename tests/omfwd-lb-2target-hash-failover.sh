#!/bin/bash
# Verifies omfwd pool.policy="hash" failover: while the hash-pinned target is
# down, messages for that key must reroute (linear-probe) to the other pool
# member with no loss; once the pinned target resumes, traffic must snap back
# to it. This is the feature's headline resilience claim
# (doc/source/configuration/modules/omfwd.rst) and had no test before this.
#
# Mirrors the bind-before-listen trick from omfwd-lb-2target-retry.sh: target2
# holds its port bound but not listening, so phase 1 is offline without a
# get_free_port race. "target-b-key" is a fixed literal chosen to hash to
# target2's index for a 2-target pool (see omfwdHashKey in tools/omfwd.c).
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export NUMMESSAGES=1000  # MUST be an EVEN number, matches the other omfwd-lb tests

TARGET2_LISTEN_RELEASE="$RSYSLOG_DYNNAME.minitcpsrvr2.listen"
TARGET2_LISTEN_READY="$RSYSLOG_DYNNAME.minitcpsrvr2.ready"
TARGET2_ACCEPT_READY="$RSYSLOG_DYNNAME.minitcpsrvr2.accepted"
rm -f "$TARGET2_LISTEN_RELEASE" "$TARGET2_LISTEN_READY" "$TARGET2_ACCEPT_READY"

start_minitcpsrvr $RSYSLOG_OUT_LOG  1
start_minitcpsrvr $RSYSLOG2_OUT_LOG 2 "$TARGET2_LISTEN_RELEASE" "$TARGET2_LISTEN_READY" "$TARGET2_ACCEPT_READY"

add_conf '
$MainMsgQueueTimeoutShutdown 10000

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
template(name="shardkey" type="string" string="target-b-key")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then {
	action(type="omfwd" target=["127.0.0.1", "127.0.0.1"]
	                    port=["'$MINITCPSRVR_PORT1'", "'$MINITCPSRVR_PORT2'"]
		protocol="tcp"
		pool.policy="hash" pool.hashkey="shardkey"
		pool.resumeInterval="1"
		action.resumeRetryCount="-1" action.resumeInterval="5")
}
'

startup

# phase 1: target2 (the pinned home for "target-b-key") is bound but not yet
# listening, so this batch must fail over via linear-probe to target1.
injectmsg 0 $NUMMESSAGES
wait_queueempty
wait_file_lines "$RSYSLOG_OUT_LOG" $NUMMESSAGES
n2_phase1=$(wc -l < $RSYSLOG2_OUT_LOG)
printf 'phase 1 (target2 down): target1=%s target2=%s (expect all on target1)\n' \
	"$(wc -l < $RSYSLOG_OUT_LOG)" "$n2_phase1"
if [ "$n2_phase1" -ne 0 ]; then
	echo "ERROR: target2 received traffic while its listener was down"
	error_exit 100
fi

# phase 2: release target2, wait until it is actually listening, wait past
# the 1-second pool retry interval, then send more of the SAME key.
touch "$TARGET2_LISTEN_RELEASE"
echo "waiting until the previously suspended pool member is listening"
wait_file_exists "$TARGET2_LISTEN_READY"
retry_ready_after=$(( $(date +%s) + 2 ))
echo "waiting until the target-pool retry interval is eligible"
while [ "$(date +%s)" -lt "$retry_ready_after" ]; do
	$TESTTOOL_DIR/msleep 100
done

injectmsg $NUMMESSAGES $NUMMESSAGES
echo "waiting until omfwd resumes target2 and the pinned key snaps back to it"
wait_file_exists "$TARGET2_ACCEPT_READY"
wait_file_lines "$RSYSLOG2_OUT_LOG" $NUMMESSAGES

shutdown_when_empty
wait_shutdown

n1=$(wc -l < $RSYSLOG_OUT_LOG)
n2=$(wc -l < $RSYSLOG2_OUT_LOG)
printf 'final: target1=%s target2=%s (expect %s / %s)\n' "$n1" "$n2" "$NUMMESSAGES" "$NUMMESSAGES"
if [ "$n1" -ne "$NUMMESSAGES" ] || [ "$n2" -ne "$NUMMESSAGES" ]; then
	echo "ERROR: expected exactly $NUMMESSAGES on each target (failover then snap-back)," \
		"got target1=$n1 target2=$n2"
	error_exit 100
fi

# and nothing is lost or duplicated across the whole run
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.log-combined"
export NUMMESSAGES=$((NUMMESSAGES * 2))
cat "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG" > "$SEQ_CHECK_FILE"
seq_check

exit_test
