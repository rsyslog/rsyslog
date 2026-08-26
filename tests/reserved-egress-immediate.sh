#!/bin/bash
# Exercise immediate shutdown while reserved egress is under sustained one-slot
# target pressure. A deliberately slow target and short queue shutdown limits
# make worker cancellation reachable. Clean bounded termination (including the
# runtime zero-reservation assertions and core checks in diag.sh) is the oracle;
# delivery count is intentionally unspecified for immediate shutdown.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(executionEngine="reservedBatch")
module(load="../plugins/omtesting/.libs/omtesting")
ruleset(name="target" queue.type="FixedArray" queue.size="1"
        queue.timeoutEnqueue="50" queue.timeoutShutdown="100"
        queue.timeoutActionCompletion="100") {
  :omtesting:sleep 1 0
}
if $msg contains "msgnum" then call target
'
startup
injectmsg 0 1000
shutdown_immediate
wait_shutdown "" 10
exit_test
