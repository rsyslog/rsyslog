#!/usr/bin/env bash
# Regression coverage for GitHub issue #4452. An unreachable Kafka broker can
# trigger delivery callbacks during shutdown; the oracle is that rsyslog exits
# cleanly without crashing while omkafka reports the failed delivery path.
. ${srcdir:=.}/diag.sh init
require_plugin omkafka
skip_TSAN "daemonized shutdown with imdiag injection forks while rsyslog threads are active"

export RSTB_DAEMONIZE="YES"
export RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT=10000
export NUMMESSAGES=2

generate_conf
add_conf '
module(load="../plugins/omkafka/.libs/omkafka")

template(name="outfmt" type="string" string="%msg%\n")

local4.* action(type="omkafka"
                topic="unreachable"
                broker="127.0.0.1:9"
                template="outfmt"
                confParam=["socket.timeout.ms=1000",
                           "message.timeout.ms=1000",
                           "reconnect.backoff.max.ms=1000",
                           "socket.keepalive.enable=true"]
                closeTimeout="1000"
                resubmitOnFailure="on"
                keepFailedMessages="on"
                failedMsgFile="'$RSYSLOG_DYNNAME'.failedmsg"
                action.resumeInterval="1"
                action.resumeRetryCount="0"
                queue.type="LinkedList"
                queue.filename="'$RSYSLOG_DYNNAME'.q"
                queue.spoolDirectory="'$RSYSLOG_DYNNAME'.spool"
                queue.saveOnShutdown="on")
'

mkdir -p "${RSYSLOG_DYNNAME}.spool"
startup
injectmsg 0 "$NUMMESSAGES"
$TESTTOOL_DIR/msleep 1500
shutdown_immediate
wait_shutdown

if [ -f "$RSYSLOG_OUT_LOG" ]; then
    check_not_present "Segmentation fault" "$RSYSLOG_OUT_LOG"
fi

exit_test
