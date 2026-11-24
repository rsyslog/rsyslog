#!/bin/bash
# Cancel a qualified omfile FE callback after a real three-byte partial write
# while a BE sibling is waiting for its shared mutex. The preload handshake
# proves that exact waiter exists before shutdown; cancellation and reused
# markers prove the sibling later reaches the real stream with its own intact
# payload. Proper daemon termination is the cleanup oracle. External output is
# deliberately ambiguous: this test makes no exact-delivery claim after cancel.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
# The preload clears LD_PRELOAD to protect daemon-spawned symbolizers. A
# Valgrind launcher would clear it before starting the daemon, preventing the
# handshake entirely. This fixture is qualified for normal and ASan runs.
if [[ ${USE_VALGRIND:-} == YES* || -n ${valgrind:-} ]]; then
    echo 'SKIP cancellation preload does not support a Valgrind launcher'
    exit 77
fi
# TSan's pthread/write interceptors cannot track cancellation unwinding through
# this uninstrumented preload shim: a standalone two-worker cleanup/unlock
# reproduction reports a double lock, while the same code without the shim is
# clean. Keep the real cancellation oracle in normal and ASan runs; do not mask
# lock reports in production code or disable TSan for other queue tests.
skip_TSAN "preload pthread/write interception misreports cancellation cleanup"
require_plugin imtcp
require_plugin imdiag

export RSYSLOG_OMFILE_CANCEL_TARGET="$PWD/$RSYSLOG_DYNNAME.sink"
export RSYSLOG_OMFILE_CANCEL_EVENTS="$PWD/$RSYSLOG_DYNNAME.events"
export RSYSLOG_PRELOAD="./.libs/liblocal_queue_omfile_cancel_preload.so"

# When ldd resolves a dynamically linked ASan runtime for this daemon, it must
# precede the cancellation shim in LD_PRELOAD. Static and non-ASan layouts have
# no libasan entry and retain the shim-only preload.
if command -v ldd >/dev/null 2>&1; then
    # Some builds place the ELF binary directly in tools; libtool wrapper
    # builds place it in .libs instead. Inspect both supported layouts.
    for daemon in ../tools/rsyslogd ../tools/.libs/rsyslogd; do
        asan_runtime=$(ldd "$daemon" 2>/dev/null |
            awk '$1 ~ /^libasan\.so/ && $2 == "=>" && $3 ~ /^\// { print $3; exit }')
        if [ -n "$asan_runtime" ] && [ -r "$asan_runtime" ]; then
            export RSYSLOG_PRELOAD="$asan_runtime:$RSYSLOG_PRELOAD"
            break
        fi
    done
fi

generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
    listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="16"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.timeoutShutdown="1" queue.timeoutActionCompletion="1"
    queue.local.frontendSize="4" queue.local.maxFrontends="1")
template(name="cancelmsg" type="string" string="%msg%\n")
if ($msg contains "cancel") then
    action(type="omfile" file="'$RSYSLOG_OMFILE_CANCEL_TARGET'" template="cancelmsg"
        queue.type="Direct" asyncWriting="off" flushOnTXEnd="on")
'
startup
tcpflood -m1 -M'<167>Mar  1 01:00:00 host tag: first-cancel'
wait_file_lines "$RSYSLOG_OMFILE_CANCEL_EVENTS" 1
content_check 'entered' "$RSYSLOG_OMFILE_CANCEL_EVENTS"
injectmsg_literal '<167>Mar  1 01:00:00 host tag sibling-after-cancel'
wait_file_lines "$RSYSLOG_OMFILE_CANCEL_EVENTS" 2
content_check 'waiting' "$RSYSLOG_OMFILE_CANCEL_EVENTS"
shutdown_immediate
wait_shutdown
content_check 'cancelled' "$RSYSLOG_OMFILE_CANCEL_EVENTS"
content_check 'reused' "$RSYSLOG_OMFILE_CANCEL_EVENTS"
exit_test
