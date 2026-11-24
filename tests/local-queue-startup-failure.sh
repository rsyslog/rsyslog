#!/bin/bash
# Inject family allocation and partially finalized worker-pool failures in
# actual local graph startup. Main, ruleset and action nodes (including global
# boundaries) must fail despite abortOnFailedQueueStartup=off, unwind constructed
# storage/slots, and stop before input activation. The -N1 validation and fault
# marker prove the fixture reached the intended boundary; the unwind marker
# proves that destruction returned without a crash or a double free. No input
# can publish messages/references because its imdiag runInput port file must
# remain absent (imtcp binds during module activation before queues start).
# Startup errors cannot use normal output routing, so stderr is the oracle.
# A fatal graph-boundary startup also must remove the PID file created before
# queue activation; absence after exit proves the cleanup path ran.
# The timeout is only a hang guard; only an ordinary exit status of one passes.
. ${srcdir:=.}/diag.sh init
timeout_cmd=${timeout_cmd:-timeout}
"$timeout_cmd" --version >/dev/null 2>&1 || timeout_cmd=gtimeout
"$timeout_cmd" --version >/dev/null 2>&1 || {
    echo 'Testbench requires GNU-compatible timeout or gtimeout'
    exit 77
}
require_plugin imtcp
require_plugin imdiag
# The injected faults live in the real local graph startup path.
skip_without_local_queue_support
startup_checker=()
pidfile="$PWD/${RSYSLOG_DYNNAME}.pid"
if [ "$USE_VALGRIND" = YES ]; then
    startup_checker=(valgrind --leak-check=full --show-leak-kinds=definite
        --errors-for-leak-kinds=definite --error-exitcode=10
        --suppressions="$srcdir/known_issues.supp")
fi

for topology in main ruleset action global-action global-ruleset global-main da-action; do
    conf="${RSYSLOG_DYNNAME}.${topology}.conf"
    portfile="$PWD/${RSYSLOG_DYNNAME}.${topology}.port"
    spool="$PWD/${RSYSLOG_DYNNAME}.${topology}.spool"
    mkdir -p "$spool"
    {
        printf 'global(abortOnFailedQueueStartup="off")\n'
        printf 'module(load="../plugins/imdiag/.libs/imdiag" listenportfilename="%s" aborttimeout="%s")\n' "$portfile" "$TB_TEST_TIMEOUT"
        printf 'input(type="imdiag" port="0")\n'
        printf 'module(load="../plugins/imtcp/.libs/imtcp")\n'
        printf 'input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="%s" ruleset="target")\n' "${portfile}.tcp"
        printf 'template(name="failurefmt" type="string" string="%%msg%%\\n")\n'
        if [ "$topology" = main ] || [ "$topology" = global-action ]; then
            printf 'main_queue(queue.scope="local" queue.type="FixedArray" queue.size="64" queue.workerThreads="2" queue.local.frontendSize="8" queue.local.maxFrontends="2")\n'
        else
            printf 'main_queue(queue.type="FixedArray" queue.size="64" queue.workerThreads="2")\n'
        fi
        if [ "$topology" = ruleset ] || [ "$topology" = global-main ]; then
            printf 'ruleset(name="target" queue.scope="local" queue.type="FixedArray" queue.size="64" queue.workerThreads="2" queue.local.frontendSize="8" queue.local.maxFrontends="2") {\n'
        elif [ "$topology" = global-ruleset ]; then
            printf 'ruleset(name="target" queue.type="FixedArray" queue.size="64" queue.workerThreads="2") {\n'
        else
            printf 'ruleset(name="target") {\n'
        fi
        if [ "$topology" = da-action ]; then
            printf 'action(name="startup-action" type="omfile" file="/dev/null" template="failurefmt" queue.scope="local" queue.type="FixedArray" queue.size="64" queue.workerThreads="2" queue.local.frontendSize="8" queue.local.maxFrontends="2" queue.filename="startup-da" queue.spoolDirectory="%s" queue.diskQueueType="segmentedDisk")\n}\n' "$spool"
        elif [ "$topology" = action ] || [ "$topology" = global-ruleset ]; then
            printf 'action(name="startup-action" type="omfile" file="/dev/null" template="failurefmt" queue.scope="local" queue.type="FixedArray" queue.size="64" queue.workerThreads="2" queue.local.frontendSize="8" queue.local.maxFrontends="2")\n}\n'
        elif [ "$topology" = global-action ]; then
            printf 'action(name="startup-action" type="omfile" file="/dev/null" template="failurefmt" queue.type="FixedArray" queue.size="64" queue.workerThreads="2")\n}\n'
        else
            printf 'action(type="omfile" file="/dev/null" template="failurefmt")\n}\n'
        fi
    } > "$conf"
    if ! ../tools/rsyslogd -N1 -f "$conf" -M"$PWD/../runtime/.libs:$PWD/../.libs" > "${conf}.validation.log" 2>&1; then
        cat "${conf}.validation.log"
        error_exit 1
    fi
    case "$topology" in
        main|global-main) pool='main Q:Reg' ;;
        ruleset|global-ruleset) pool='target:Reg' ;;
        action|global-action|da-action) pool='startup-action queue:Reg' ;;
    esac
    case "$topology" in
        global-*) faults='startup-pool' ;;
        da-action) faults='da-pool da-child' ;;
        action) faults='startup-family startup-frontend startup-pool frontend-pool' ;;
        *) faults='startup-family startup-frontend startup-pool' ;;
    esac
    for fault in $faults; do
        faultenv="$fault"
        poolenv=''
        if [ "$fault" = startup-pool ] || [ "$fault" = frontend-pool ]; then
            faultenv=''
            poolenv="$pool"
            [ "$fault" != frontend-pool ] || poolenv='local-fe-2'
        fi
        if [ "$fault" = da-pool ] || [ "$fault" = da-child ]; then
            faultenv=''
            poolenv='startup-action queue:DAwpool'
            [ "$fault" != da-child ] || poolenv='startup-action queue[DA]:Reg'
        fi
        log="${RSYSLOG_DYNNAME}.${topology}.${fault}.log"
        RSYSLOG_LOCAL_QUEUE_TEST_FAULT="$faultenv" RSYSLOG_LOCAL_QUEUE_TEST_STARTUP_POOL="$poolenv" "$timeout_cmd" -k 2 "$TB_TEST_TIMEOUT" \
            "${startup_checker[@]}" ../tools/rsyslogd -n -f "$conf" -i "$pidfile" \
            -M"$PWD/../runtime/.libs:$PWD/../.libs" > "$log" 2>&1
        result=$?
        if [ "$result" -ne 1 ] || [ -e "$portfile" ] || [ -e "$pidfile" ] ||
            ! grep -F "local queue test fault: ${faultenv:-startup-pool $poolenv}" "$log" > /dev/null ||
            ! grep -F 'local queue test: startup unwind complete' "$log" > /dev/null; then
            echo "FAIL: $topology/$fault did not fail before input activation (status $result)"
            cat "$log"
            error_exit 1
        fi
        content_check 'run failed with error -6' "$log"
        case "$topology" in
            action|global-action|da-action)
                content_check "local queue graph: cannot start action queue 'startup-action' before input startup" "$log"
                ;;
        esac
    done
done

# A failed pool outside a graph retains the legacy disabled-action policy.
# Harness startup proves input release, surviving output proves reference
# delivery, and wait_shutdown checks the daemon's clean termination marker.
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="failurefmt" type="string" string="%msg%\n")
action(name="startup-action" type="omfile" file="/dev/null" template="failurefmt"
       queue.type="FixedArray" queue.size="64" queue.workerThreads="2")
if $msg contains "msgnum:" then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="failurefmt")
'
log="${RSYSLOG_DYNNAME}.legacy.log"
export RSYSLOG_LOCAL_QUEUE_TEST_STARTUP_POOL='startup-action queue:Reg'
startup > "$log" 2>&1
tcpflood -m 1
wait_file_lines "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown
unset RSYSLOG_LOCAL_QUEUE_TEST_STARTUP_POOL
content_check 'local queue test fault: startup-pool startup-action queue:Reg' "$log"
content_check 'error starting up action queue' "$log"
content_check 'msgnum:00000000' "$RSYSLOG_OUT_LOG"
exit_test
