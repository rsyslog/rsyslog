#!/bin/bash
# Regression coverage for ommail subject hardening in SMTP mode. A
# message-controlled JSON field is decoded by mmjsonparse and rendered by
# subject.template. The oracle is the captured SMTP DATA transcript: CR/LF from
# the rendered subject must stay inside the Subject header value and must not
# create a new RFC822 header or premature header/body boundary.
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
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/ommail/.libs/ommail")

template(name="mailBody" type="string" string="body %msg%\\r\\n")
template(name="mailSubject" type="string" string="%$!subject%")

if $rawmsg contains "trigger-ommail-subject" then {
	action(type="mmjsonparse" cookie="")
	action(
		type="ommail"
		mode="smtp"
		server="127.0.0.1"
		port="'$SMTP_PORT'"
		mailfrom="rsyslog@example.net"
		mailto="operator@example.net"
		subject.template="mailSubject"
		body.enable="on"
		template="mailBody"
	)
}
'

startup
injectmsg_literal '<167>1 2003-08-24T05:14:15.000003-07:00 host app proc msgid - {"subject":"legit\r\nX-Injected-Header: injected\r\n\r\nInjected pre-body","trigger":"trigger-ommail-subject"}'
shutdown_when_empty
wait_shutdown

wait_file_exists "$SMTP_DONE"
wait "$SMTP_PID"
trap - EXIT

content_check "DATA_BEGIN" "$SMTP_OUT"
content_check "Subject: legit  X-Injected-Header: injected    Injected pre-body" "$SMTP_OUT"
if grep -Fx -- "X-Injected-Header: injected" "$SMTP_OUT" >/dev/null; then
	error_exit 1
fi
if grep -Fx -- "Injected pre-body" "$SMTP_OUT" >/dev/null; then
	error_exit 1
fi
content_check "DATA_END" "$SMTP_OUT"

if [ -n "${OMMAIL_SMTP_REPRO_CAPTURE:-}" ]; then
	cp "$SMTP_OUT" "$OMMAIL_SMTP_REPRO_CAPTURE"
fi

exit_test
