#!/bin/bash
# Inject local family allocation failures in the real startup path. Both main
# and ruleset local queues must terminate with the explicit startup-failure
# diagnostic, despite abortOnFailedQueueStartup=off. A preceding successful -N1
# and the test-hook marker rule out a malformed fixture as a false positive.
# No input port file may appear: fallback to a usable Direct queue is forbidden.
# The timeout is only a hang guard; only an ordinary exit status of one passes.
. ${srcdir:=.}/diag.sh init
timeout_cmd=${timeout_cmd:-timeout}
command -v "$timeout_cmd" >/dev/null 2>&1 || timeout_cmd=gtimeout
command -v "$timeout_cmd" >/dev/null 2>&1 || error_exit 77 'no timeout command available'
require_plugin imtcp

for topology in main ruleset; do
    conf="${RSYSLOG_DYNNAME}.${topology}.conf"
    portfile="$PWD/${RSYSLOG_DYNNAME}.${topology}.port"
    {
        printf 'global(abortOnFailedQueueStartup="off")\n'
        printf 'module(load="../plugins/imtcp/.libs/imtcp")\n'
        printf 'input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="%s" ruleset="target")\n' "$portfile"
        printf 'template(name="failurefmt" type="string" string="%%msg%%\\n")\n'
        if [ "$topology" = main ]; then
            printf 'main_queue(queue.scope="local" queue.type="FixedArray" queue.size="64" queue.local.frontendSize="8" queue.local.maxFrontends="2")\n'
            printf 'ruleset(name="target") {\n'
        else
            printf 'main_queue(queue.type="FixedArray" queue.size="64")\n'
            printf 'ruleset(name="target" queue.scope="local" queue.type="FixedArray" queue.size="64" queue.local.frontendSize="8" queue.local.maxFrontends="2") {\n'
        fi
        printf 'action(type="omfile" file="/dev/null" template="failurefmt")\n}\n'
    } > "$conf"
    if ! ../tools/rsyslogd -N1 -f "$conf" -M../runtime/.libs:../.libs > "${conf}.validation.log" 2>&1; then
        cat "${conf}.validation.log"
        error_exit 1
    fi
    for fault in startup-family startup-frontend; do
        log="${RSYSLOG_DYNNAME}.${topology}.${fault}.log"
        RSYSLOG_LOCAL_QUEUE_TEST_FAULT="$fault" $timeout_cmd -k 2 "$TB_TEST_TIMEOUT" \
            ../tools/rsyslogd -n -f "$conf" -i "${RSYSLOG_DYNNAME}.pid" \
            -M../runtime/.libs:../.libs > "$log" 2>&1
        result=$?
        if [ "$result" -ne 1 ] || [ -e "$portfile" ] ||
            ! grep -F "local queue test fault: $fault" "$log" > /dev/null ||
            ! grep -F 'local queue safety or abortOnFailedQueueStartup requires aborting' "$log" > /dev/null; then
            echo "FAIL: $topology/$fault did not fail before input activation (status $result)"
            cat "$log"
            error_exit 1
        fi
    done
done
exit_test
