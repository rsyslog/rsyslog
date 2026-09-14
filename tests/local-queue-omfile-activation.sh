#!/bin/bash
# -N1 must validate local omfile settings without creating output directories
# or files. Actual startup must reject an unusable/nonregular opened sink before
# inputs run. The process exit code and startup diagnostic are the oracles;
# timeout only protects against accidental blocking FIFO opens and never counts
# as an expected failure. These pre-input errors cannot use normal omfile logs.
. ${srcdir:=.}/diag.sh init

SINK="$PWD/$RSYSLOG_DYNNAME.newdir/sink"
CONF="$PWD/$RSYSLOG_DYNNAME.activation.conf"
LOG="$PWD/$RSYSLOG_DYNNAME.activation.log"
write_config() {
    cat > "$CONF" <<CONFIG
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="16"
    queue.local.frontendSize="4" queue.local.maxFrontends="1")
template(name="activationmsg" type="string" string="%msg%\\n")
action(type="omfile" file="$1" template="activationmsg" queue.type="Direct"
    asyncWriting="off" flushOnTXEnd="on")
CONFIG
}
write_config "$SINK"
../tools/rsyslogd -C -N1 -f "$CONF" -M"$RSYSLOG_MODDIR" > "$LOG" 2>&1 || error_exit 1
if [ -e "$PWD/$RSYSLOG_DYNNAME.newdir" ]; then
    error_exit 1 'configuration validation created the omfile parent directory'
fi

FIFO="$PWD/$RSYSLOG_DYNNAME.fifo"
mkfifo "$FIFO"
for sink in "$FIFO" /dev/full "/proc/$RSYSLOG_DYNNAME/sink"; do
    write_config "$sink"
    ../tools/rsyslogd -C -N1 -f "$CONF" -M"$RSYSLOG_MODDIR" > "$LOG" 2>&1 || error_exit 1
    timeout -k 2 15 ../tools/rsyslogd -C -n -i "$RSYSLOG_DYNNAME.fail.pid" \
        -f "$CONF" -M"$RSYSLOG_MODDIR" > "$LOG" 2>&1
    status=$?
    if [ "$status" -eq 0 ] || [ "$status" -ge 128 ] || [ "$status" -eq 124 ]; then
        cat "$LOG"
        error_exit 1 "expected prompt ordinary startup failure, got $status for $sink"
    fi
    content_check 'local omfile' "$LOG"
done
exit_test
