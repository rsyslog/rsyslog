#!/bin/bash
# This test covers ommail's SMTP mode against a deterministic local SMTP
# helper. The helper writes its port file only after listen() succeeds and
# writes a done file after QUIT, so success is proven by synchronized shutdown
# plus the captured SMTP transcript instead of fixed sleeps.

. ${srcdir:=.}/diag.sh init

if ! command -v python3 >/dev/null 2>&1; then
	echo "python3 not available, skipping ommail SMTP helper test"
	skip_test
fi

PORT_FILE="$RSYSLOG_DYNNAME.ommail-smtp.port"
SMTP_OUT="$RSYSLOG_DYNNAME.ommail-smtp.out"
SMTP_DONE="$RSYSLOG_DYNNAME.ommail-smtp.done"

python3 "$srcdir/testsuites/ommail-smtp-server.py" \
	--port-file "$PORT_FILE" \
	--output "$SMTP_OUT" \
	--done-file "$SMTP_DONE" &
SMTP_PID=$!
trap 'kill "$SMTP_PID" 2>/dev/null' EXIT

wait_file_exists "$PORT_FILE"
SMTP_PORT=$(cat "$PORT_FILE")

generate_conf
add_conf '
module(load="../plugins/ommail/.libs/ommail")

template(name="mailBody" type="string" string="smtp alert %msg%\\r\\n")

if $msg contains "msgnum:" then {
    action(
        type="ommail"
        mode="smtp"
        server="127.0.0.1"
        port="'$SMTP_PORT'"
        mailfrom="rsyslog@example.net"
        mailto=["operator@example.net", "admin@example.net"]
        subject.text="rsyslog smtp alert"
        body.enable="on"
        template="mailBody"
    )
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

wait_file_exists "$SMTP_DONE"
wait "$SMTP_PID"
trap - EXIT

content_check "CMD:HELO" "$SMTP_OUT"
content_check "CMD:MAIL FROM:<rsyslog@example.net>" "$SMTP_OUT"
content_check "CMD:RCPT TO:<operator@example.net>" "$SMTP_OUT"
content_check "CMD:RCPT TO:<admin@example.net>" "$SMTP_OUT"
content_check "CMD:DATA" "$SMTP_OUT"
content_check "DATA_BEGIN" "$SMTP_OUT"
content_check "From: <rsyslog@example.net>" "$SMTP_OUT"
content_check "Subject: rsyslog smtp alert" "$SMTP_OUT"
content_check "smtp alert  msgnum:00000000:" "$SMTP_OUT"
content_check "DATA_END" "$SMTP_OUT"
content_check "CMD:QUIT" "$SMTP_OUT"

exit_test
