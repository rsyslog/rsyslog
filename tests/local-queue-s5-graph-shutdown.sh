#!/bin/bash
# Extend the reverse-declared shutdown fixture across a real downstream DA
# boundary. Root ID1 remains owned upstream while leaf FE/BE/DA callbacks hold
# IDs0/3/4; the disk store must exist before shutdown. Releasing callbacks only
# after graph shutdown entry checks producer authority and destination lifetime.
# Establish the leaf BE callback before crossing its DA watermark; otherwise
# the DA consumer may claim that barrier and block before reaching ID4.
# The root FE holds the spill burst so its blocked BE cannot intercept it. Shutdown may execute or persist leaf
# work under the existing DA deadline policy. A no-ingress restart reads the
# same leaf spool with barriers disabled; exact combined 0..65 detects loss or
# replay of already downstream-owned IDs.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting
export NUMMESSAGES=66
engine=${LOCAL_QUEUE_S5_ENGINE:-disk}
SPOOL="$PWD/$RSYSLOG_DYNNAME.spool"
mkdir -p "$SPOOL"
DA_ENTER="$PWD/$RSYSLOG_DYNNAME.da-enter"
DA_RELEASE="$PWD/$RSYSLOG_DYNNAME.da-release"
STATSFILE="$PWD/$RSYSLOG_DYNNAME.stats"
ROOT_ENTER="$PWD/$RSYSLOG_DYNNAME.root-enter"
ROOT_RELEASE="$PWD/$RSYSLOG_DYNNAME.root-release"
FE_ENTER="$PWD/$RSYSLOG_DYNNAME.fe-enter"
FE_RELEASE="$PWD/$RSYSLOG_DYNNAME.fe-release"
BE_ENTER="$PWD/$RSYSLOG_DYNNAME.be-enter"
BE_RELEASE="$PWD/$RSYSLOG_DYNNAME.be-release"
export RSYSLOG_LOCAL_QUEUE_TEST_GRAPH_SHUTDOWN_FILE="$PWD/$RSYSLOG_DYNNAME.graph-stop"
mkfifo "$ROOT_RELEASE" "$FE_RELEASE" "$BE_RELEASE" "$DA_RELEASE"
exec {da_release_fd}<>"$DA_RELEASE"
# Keep open() out of the callback's signal window: graceful worker shutdown
# sends a wake signal. The barrier read retries EINTR, whereas FIFO open does
# not. Owned RDWR handles also ensure a failed callback cannot hang the shell.
exec {root_release_fd}<>"$ROOT_RELEASE"
exec {fe_release_fd}<>"$FE_RELEASE"
exec {be_release_fd}<>"$BE_RELEASE"
generate_conf
localq_make_startup_marker_absolute
add_conf '
global(workDirectory="'$SPOOL'" processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/omtesting/.libs/omtesting")
template(name="ids" type="string" string="%msg:F,58:2%\n")
ruleset(name="leaf" queue.type="FixedArray" queue.scope="local" queue.size="32" queue.filename="s5leaf"
    queue.highWatermark="8" queue.lowWatermark="4" queue.diskQueueType="'$engine'" queue.saveOnShutdown="on"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.timeoutShutdown="10000" queue.timeoutActionCompletion="1000"
    queue.local.frontendSize="1" queue.local.maxFrontends="2" queue.local.helperBatchSize="0") {
    if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$FE_ENTER' '$FE_RELEASE';ids
    if ($msg contains "msgnum:00000003:") then :omtesting:file_barrier '$BE_ENTER' '$BE_RELEASE';ids
    if ($msg contains "msgnum:00000004:") then :omtesting:file_barrier '$DA_ENTER' '$DA_RELEASE';ids
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids")
}
main_queue(queue.type="FixedArray" queue.scope="local" queue.size="32"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.timeoutShutdown="10000" queue.timeoutActionCompletion="1000"
    queue.local.frontendSize="128" queue.local.maxFrontends="1" queue.local.helperBatchSize="0")
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="1"
    listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
if ($msg contains "msgnum:") then {
    if ($msg contains "msgnum:00000001:") then :omtesting:file_barrier '$ROOT_ENTER' '$ROOT_RELEASE';ids
    call leaf
}
'
startup
tcpflood -m1 -i0
wait_file_lines "$FE_ENTER" 1
injectmsg 1 1
wait_file_lines "$ROOT_ENTER" 1
tcpflood -m2 -i2
wait_file_lines "$BE_ENTER" 1
# IDs2/3 are below the high watermark, and BE now demonstrably owns ID3.
tcpflood -m62 -i4
wait_file_lines "$DA_ENTER" 1
if [ "$engine" = disk ]; then
    compgen -G "$SPOOL/s5leaf.[0-9]*" >/dev/null || error_exit 1 'downstream classic DA store missing'
else
    [ -e "$SPOOL/s5leaf.segq/state" ] || error_exit 1 'downstream segmented DA store missing'
fi
shutdown_immediate
wait_file_exists "$RSYSLOG_LOCAL_QUEUE_TEST_GRAPH_SHUTDOWN_FILE"
printf 'release\n' >&"$fe_release_fd"
printf 'release\n' >&"$be_release_fd"
printf 'release\n' >&"$root_release_fd"
printf 'release\n' >&"$da_release_fd"
wait_shutdown
exec {da_release_fd}>&-
exec {root_release_fd}>&-
exec {fe_release_fd}>&-
exec {be_release_fd}>&-
# Keep queue identity and destination unchanged. Pending messages already
# belong to the leaf and must recover there without re-injecting root input.
# Saved callbacks may include a formerly gated ID, so remove only the gates.
sed -i '/:omtesting:file_barrier/d' "$CONF_FILE"
startup
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
seq_check
exit_test
