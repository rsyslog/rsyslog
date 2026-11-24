#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise real queue admission after publishing the one-way input termination
# flag. The imdiag fixture gates the FE singleton before filling the real
# backend, so its FE worker cannot borrow BE work in place of the singleton.
# It gates the dedicated callback and acknowledges an input capacity wait
# before the real graph wake. Input
# FORCE_TERM must complete while callbacks still hold all capacity. A real WTI
# then hands an accepted lease to the full destination and succeeds after gate
# release. Observer refs and local accounting prove conservation; elapsed time
# is solely a hang watchdog. Both legacy global and local FE no-fit/BE fallback
# ownership are checked. The fixture ends input service, so signal shutdown and
# the proper-termination marker are the daemon exit oracle.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imdiag
require_plugin impstats

local_main_params='queue.local.frontendSize="4" queue.local.maxFrontends="1"'
if [ "${LOCAL_QUEUE_INPUT_TERM_SCOPE:-local}" = global ]; then local_main_params=; fi
generate_conf
localq_make_startup_marker_absolute
add_conf '
global(processInternalMessages="off")
module(load="../plugins/impstats/.libs/impstats" interval="60000" log.syslog="off"
    log.file="'"$PWD/$RSYSLOG_DYNNAME"'.stats")
main_queue(queue.scope="'"${LOCAL_QUEUE_INPUT_TERM_SCOPE:-local}"'" queue.type="FixedArray" queue.size="16"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.timeoutEnqueue="60000" '"$local_main_params"')
template(name="inputtermfmt" type="string" string="%msg%\n")
action(type="omfile" file="/dev/null" template="inputtermfmt" queue.scope="local" queue.type="FixedArray" queue.size="16"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.local.frontendSize="4" queue.local.maxFrontends="1" asyncWriting="off" flushOnTXEnd="on")
'
startup
response=$(printf "localqueueinputtermtest\n" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
case "$response" in
    *"OK input=FORCE_TERM backend=FORCE_TERM global=FORCE_TERM handoff=OK refs=conserved"*) ;;
    *) echo "FAIL: unexpected input termination response: $response"; error_exit 1 ;;
esac
shutdown_immediate
wait_shutdown
exit_test
