#!/bin/bash
# Exercise concurrent error reporting and first action suspension under TSan.
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
