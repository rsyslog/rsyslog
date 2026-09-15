#!/bin/bash
# S5 persistence uses actual FE inflight/queued work plus BE/DA spill. Every
# data callback stops before its destination write, so no ID can be delivered
# before persistence and exact-once final inventory is a valid oracle (unlike
# an interrupted write). Two shutdowns, including one during recovery, retain
# the logical spool while worker counts and local/global scope change. Final
# ungated recovery must deliver exactly 0..65, without replaying input roots.
# The 41 held FE IDs exceed BE capacity32: final consolidation must fill the
# joined BE and invoke its existing save worker before accepting the suffix.
# Barrier entry, FE inventory and the actual disk store are phase predicates;
# shutdown timeouts merely force the existing cooperative/cancellation path.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting
export NUMMESSAGES=66
engine=${LOCAL_QUEUE_S5_ENGINE:-disk}
SPOOL="$PWD/$RSYSLOG_DYNNAME.spool"
mkdir -p "$SPOOL"
STATS="$PWD/$RSYSLOG_DYNNAME.stats"
RELEASE="$PWD/$RSYSLOG_DYNNAME.release"
mkfifo "$RELEASE"
# Avoid FIFO open() being interrupted before the cancel-safe read starts.
exec {release_fd}<>"$RELEASE"

write_phase() {
    local phase="$1" scope="$2" workers="$3" blocked="$4"
    local local_config='' barrier_config=''
    ENTRY="$PWD/$RSYSLOG_DYNNAME.entry-$phase"
    if [ "$scope" = local ]; then
        local_config='queue.scope="local" queue.local.frontendSize="64" queue.local.maxFrontends="4" queue.local.helperBatchSize="0"'
    fi
    if [ "$scope" = local ] && [ "$blocked" = no ]; then
        local_config='queue.scope="local" queue.local.frontendSize="64" queue.local.maxFrontends="4"'
    fi
    if [ "$blocked" = yes ]; then
        barrier_config=':omtesting:file_barrier '"$ENTRY $RELEASE"';ids'
    fi
    generate_conf
    localq_make_startup_marker_absolute
    add_conf '
global(workDirectory="'$SPOOL'" processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" interval="1" log.syslog="off" log.file="'$STATS'")
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="1"
 listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
main_queue(queue.type="FixedArray" queue.filename="s5queue" queue.size="32"
 queue.highWatermark="8" queue.lowWatermark="4" queue.workerThreads="'$workers'"
 queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
 queue.timeoutShutdown="100" queue.timeoutActionCompletion="100" queue.saveOnShutdown="on"
 queue.diskQueueType="'$engine'" '"$local_config"')
template(name="ids" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
 '"$barrier_config"'
 action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids" asyncWriting="off" flushOnTXEnd="on")
}
'
}

wait_store() {
    local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT ))
    while [ "$(date +%s)" -le "$deadline" ]; do
        if [ "$engine" = disk ]; then
            compgen -G "$SPOOL/s5queue.[0-9]*" >/dev/null && return
        else
            [ -e "$SPOOL/s5queue.segq/state" ] && return
        fi
        "$TESTTOOL_DIR/msleep" 50
    done
    error_exit 1 'DA store did not materialize while callbacks were held'
}

assert_saved() {
    if [ "$engine" = disk ]; then
        [ -s "$SPOOL/s5queue.qi" ] || error_exit 1 'classic save has no state file'
    else
        [ -s "$SPOOL/s5queue.segq/state" ] || error_exit 1 'segmented save has no state file'
    fi
    [ ! -s "$RSYSLOG_OUT_LOG" ] || error_exit 1 'a blocked data callback wrote before recovery'
}

write_phase first local 1 yes
startup
tcpflood -m1 -i0
wait_file_lines "$ENTRY" 1
tcpflood -m40 -i1
localq_wait_stats "$STATS" 'main Q.local' 'fe.queued.messages=40' 'fe.inflight.messages=1'
injectmsg 41 25
wait_store
STOPMARK="$PWD/$RSYSLOG_DYNNAME.first-stop"
response=$(printf 'localqueuestopcheck %s\n' "$STOPMARK" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")
case "$response" in *OK*) ;; *) error_exit 1 'stop checker arm failed' ;; esac
shutdown_immediate
wait_shutdown
assert_saved
wait_file_lines "$STOPMARK" 1
custom_content_check 'restored=0 persisted=66 executed=0 discarded=0' "$STOPMARK"

# Both engines exercise local -> global -> local with changed BE worker
# counts and default helping enabled on final recovery.
second_scope=global
final_scope=local
write_phase second "$second_scope" 2 yes
startup
wait_file_lines "$ENTRY" 1
shutdown_immediate
wait_shutdown
assert_saved

write_phase final "$final_scope" 3 no
startup
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
STOPMARK="$PWD/$RSYSLOG_DYNNAME.final-stop"
response=$(printf 'localqueuestopcheck %s\n' "$STOPMARK" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")
case "$response" in *OK*) ;; *) error_exit 1 'recovery stop checker arm failed' ;; esac
shutdown_when_empty
wait_shutdown
seq_check
wait_file_lines "$STOPMARK" 1
custom_content_check 'restored=66 persisted=0 executed=66 discarded=0' "$STOPMARK"
exec {release_fd}>&-
exit_test
