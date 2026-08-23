#!/bin/bash
# Exercise concurrent error reporting and first action suspension under TSan.
# The two-message workload and two-worker main queue cause both actions to enter
# the barrier; it releases only after both are present, exposing concurrent
# config-default resolution. A ThreadSanitizer report fails the test; a clean,
# synchronized shutdown proves the actions completed without one.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
main_queue(queue.workerThreads="2"
	queue.workerThreadMinimumMessages="1"
	queue.dequeueBatchSize="1")

module(load="../plugins/omtesting/.libs/omtesting")

$ActionResumeRetryCount 0
$ActionResumeInterval 1
:msg, contains, "msgnum:" :omtesting:barrier_error 2
& :omtesting:barrier_suspend 2
'

startup
injectmsg 0 2
shutdown_when_empty
wait_shutdown
exit_test
