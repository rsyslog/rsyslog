#!/bin/bash
# Actual producer FE0 is blocked on ID0 and holds queued ID1. BE is held on ID2;
# the disk consumer reaches ID3 after the remaining burst triggers runtime DA.
# Before releasing these callbacks, a disk segment/state object must exist.
# Exact destination IDs after drain prove no FE/BE/DA loss or duplicate. This
# is runtime spill evidence, not a timing or filesystem-size throughput oracle.
# Helping is enabled: the held FE prevents precondition theft, then becomes
# eligible to borrow memory work while DA finishes after release. Save-disabled
# wrappers instead hold FE IDs0/1 through shutdown and require exact 2..65 after
# restart, proving the pending FE IDs were discarded without replay.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting
export NUMMESSAGES=66
engine=${LOCAL_QUEUE_S5_ENGINE:-disk}
save_on_shutdown=${LOCAL_QUEUE_S5_SAVE:-on}
SPOOL="$PWD/$RSYSLOG_DYNNAME.spool"
mkdir -p "$SPOOL"
STATS="$PWD/$RSYSLOG_DYNNAME.stats"
for role in FE BE DA; do
    declare "${role}_ENTRY=$PWD/$RSYSLOG_DYNNAME.$role.entry"
    declare "${role}_RELEASE=$PWD/$RSYSLOG_DYNNAME.$role.release"
done
mkfifo "$FE_RELEASE" "$BE_RELEASE" "$DA_RELEASE"
exec {fe_fd}<>"$FE_RELEASE"
exec {be_fd}<>"$BE_RELEASE"
exec {da_fd}<>"$DA_RELEASE"
generate_conf
localq_make_startup_marker_absolute
add_conf '
global(workDirectory="'$SPOOL'" processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" interval="1" log.syslog="off" log.file="'$STATS'")
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="1"
 listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
main_queue(queue.scope="local" queue.type="FixedArray" queue.filename="s5queue" queue.size="32"
 queue.highWatermark="8" queue.lowWatermark="4" queue.workerThreads="1" queue.workerThreadMinimumMessages="1"
 queue.dequeueBatchSize="1" queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.helperBatchSize="1"
 queue.diskQueueType="'$engine'" queue.saveOnShutdown="'$save_on_shutdown'"
 queue.timeoutShutdown="100" queue.timeoutActionCompletion="100")
template(name="ids" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$FE_ENTRY' '$FE_RELEASE';ids
if ($msg contains "msgnum:00000002:") then :omtesting:file_barrier '$BE_ENTRY' '$BE_RELEASE';ids
if ($msg contains "msgnum:00000003:") then :omtesting:file_barrier '$DA_ENTRY' '$DA_RELEASE';ids
if ($msg contains "msgnum:") then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids")
'
startup
tcpflood -m1 -i0
wait_file_lines "$FE_ENTRY" 1
tcpflood -m1 -i1
localq_wait_stats "$STATS" 'main Q.local' 'fe.queued.messages=1' 'fe.inflight.messages=1'
injectmsg 2 1
wait_file_lines "$BE_ENTRY" 1
injectmsg 3 63
wait_file_lines "$DA_ENTRY" 1
if [ "$engine" = disk ]; then
    compgen -G "$SPOOL/s5queue.[0-9]*" >/dev/null || error_exit 1 'classic runtime spill missing'
else
    [ -e "$SPOOL/s5queue.segq/state" ] || error_exit 1 'segmented runtime spill missing'
fi
if [ "$save_on_shutdown" = on ]; then printf 'release\n' >&"$fe_fd"; fi
printf 'release\n' >&"$be_fd"
printf 'release\n' >&"$da_fd"
if [ "$save_on_shutdown" = off ]; then
    # IDs0/1 remain FE inflight/queued. Every BE/DA ID is already delivered,
    # making exactly these two IDs the save-disabled loss oracle. The timeout
    # invokes normal callback cancellation; it is not a success threshold.
    wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 64
    STOPMARK="$PWD/$RSYSLOG_DYNNAME.stop"
    response=$(printf 'localqueuestopcheck %s\n' "$STOPMARK" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")
    case "$response" in *OK*) ;; *) error_exit 1 'stop checker arm failed' ;; esac
    shutdown_immediate
    wait_shutdown
    wait_file_lines "$STOPMARK" 1
    custom_content_check 'restored=0 persisted=0 executed=64 discarded=2' "$STOPMARK"
    seq_check 2 65
    # No new ingress: an empty recovery must not replay those discarded IDs.
    startup
    shutdown_when_empty
    wait_shutdown
    seq_check 2 65
else
    wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
    shutdown_when_empty
    wait_shutdown
    seq_check
fi
exec {fe_fd}>&-
exec {be_fd}>&-
exec {da_fd}>&-
exit_test
