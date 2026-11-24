#!/bin/bash
# -N1 must validate local omfile settings without creating output directories
# or files. Actual startup must reject an unusable/nonregular opened sink before
# inputs run. The process exit code and startup diagnostic are the oracles;
# timeout only protects against accidental blocking FIFO opens and never counts
# as an expected failure. These pre-input errors cannot use normal omfile logs.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
timeout_cmd=${timeout_cmd:-timeout}
"$timeout_cmd" --version >/dev/null 2>&1 || timeout_cmd=gtimeout
"$timeout_cmd" --version >/dev/null 2>&1 || {
    echo 'Testbench requires GNU-compatible timeout or gtimeout'
    exit 77
}

SINK="$PWD/$RSYSLOG_DYNNAME.newdir/sink"
CONF="$PWD/$RSYSLOG_DYNNAME.activation.conf"
LOG="$PWD/$RSYSLOG_DYNNAME.activation.log"
if [ "${LOCAL_OMFILE_ACTIVATION_YAML:-0}" -eq 1 ]; then
    require_yaml_support
    CONF="$PWD/$RSYSLOG_DYNNAME.activation.yaml"
fi
write_config() {
    if [ "${LOCAL_OMFILE_ACTIVATION_YAML:-0}" -eq 1 ]; then
        cat > "$CONF" <<YAML
version: 2
mainqueue:
  queue.scope: local
  queue.type: FixedArray
  queue.size: 16
  queue.local.frontendSize: 4
  queue.local.maxFrontends: 1
templates:
  - name: activationmsg
    type: string
    string: "%msg%\\n"
rulesets:
  - name: main
    statements:
      - type: omfile
        file: "$1"
        template: activationmsg
        queue.type: Direct
        asyncWriting: "off"
        flushOnTXEnd: "on"
YAML
        return
    fi
    cat > "$CONF" <<CONFIG
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="16"
    queue.local.frontendSize="4" queue.local.maxFrontends="1")
template(name="activationmsg" type="string" string="%msg%\\n")
action(type="omfile" file="$1" template="activationmsg" queue.type="Direct"
    asyncWriting="off" flushOnTXEnd="on")
CONFIG
}
write_config "$SINK"
"$timeout_cmd" -k 2 15 ../tools/rsyslogd -C -N1 -f "$CONF" -M"$RSYSLOG_MODDIR" > "$LOG" 2>&1 || error_exit 1
if [ -e "$PWD/$RSYSLOG_DYNNAME.newdir" ]; then
    error_exit 1 'configuration validation created the omfile parent directory'
fi

# A nondefault compression driver is outside the plain-stream lifecycle audit
# even when the action's compression level is zero. Test native module/action
# inheritance, and legacy selector inheritance separately in RainerScript.
write_config "$SINK"
if [ "${LOCAL_OMFILE_ACTIVATION_YAML:-0}" -eq 1 ]; then
    cat >> "$CONF" <<'YAML'
modules:
  - load: builtin:omfile
    compression.driver: zstd
YAML
else
    sed -i.localq '1i\
module(load="builtin:omfile" compression.driver="zstd")' "$CONF" || error_exit $?
    rm -f "$CONF.localq"
fi
if "$timeout_cmd" -k 2 15 ../tools/rsyslogd -C -N1 -f "$CONF" -M"$RSYSLOG_MODDIR" > "$LOG" 2>&1; then
    error_exit 1 'local omfile accepted an unaudited compression driver'
fi
content_check 'experimental local queue configuration rejected' "$LOG"
if [ -e "$PWD/$RSYSLOG_DYNNAME.newdir" ]; then
    error_exit 1 'rejected compression driver created output state'
fi
if [ "${LOCAL_OMFILE_ACTIVATION_YAML:-0}" -eq 0 ]; then
    write_config "$SINK"
    sed -i.localq '/^action(type=/,$d' "$CONF" || error_exit $?
    rm -f "$CONF.localq"
    printf '*.* %s;activationmsg\n' "$SINK" >> "$CONF"
    "$timeout_cmd" -k 2 15 ../tools/rsyslogd -C -N1 -f "$CONF" -M"$RSYSLOG_MODDIR" > "$LOG" 2>&1 || error_exit 1
    sed -i.localq '1i\
module(load="builtin:omfile" compression.driver="zstd")' "$CONF" || error_exit $?
    rm -f "$CONF.localq"
    if "$timeout_cmd" -k 2 15 ../tools/rsyslogd -C -N1 -f "$CONF" -M"$RSYSLOG_MODDIR" > "$LOG" 2>&1; then
        error_exit 1 'legacy omfile ignored inherited compression driver'
    fi
    content_check 'experimental local queue configuration rejected' "$LOG"
fi

FIFO="$PWD/$RSYSLOG_DYNNAME.fifo"
mkfifo "$FIFO"
NOTDIR="$PWD/$RSYSLOG_DYNNAME.notdir"
printf 'regular file, not a directory\n' > "$NOTDIR"
for sink in "$FIFO" /dev/zero "$NOTDIR/sink"; do
    write_config "$sink"
    "$timeout_cmd" -k 2 15 ../tools/rsyslogd -C -N1 -f "$CONF" -M"$RSYSLOG_MODDIR" > "$LOG" 2>&1 || error_exit 1
    "$timeout_cmd" -k 2 15 ../tools/rsyslogd -C -n -i "$RSYSLOG_DYNNAME.fail.pid" \
        -f "$CONF" -M"$RSYSLOG_MODDIR" > "$LOG" 2>&1
    status=$?
    if [ "$status" -eq 0 ] || [ "$status" -ge 128 ] || [ "$status" -eq 124 ]; then
        cat "$LOG"
        error_exit 1 "expected prompt ordinary startup failure, got $status for $sink"
    fi
    content_check 'cannot prepare qualified omfile' "$LOG"
done
exit_test
