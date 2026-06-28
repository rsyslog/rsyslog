#!/bin/bash
# Regression coverage for ommail subject hardening in sendmail mode. A
# message-controlled JSON field is decoded by mmjsonparse and rendered by
# subject.template. The oracle is the captured sendmail stdin: CR/LF from the
# rendered subject must stay inside the Subject header value and must not create
# a new RFC822 header or premature header/body boundary.
. ${srcdir:=.}/diag.sh init

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
		mode="sendmail"
		sendmail.binary="'$srcdir'/testsuites/ommail-sendmail-bin.sh"
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

content_check "Subject: legit  X-Injected-Header: injected    Injected pre-body"
if grep -Fx -- "X-Injected-Header: injected" "$RSYSLOG_OUT_LOG" >/dev/null; then
	error_exit 1
fi
if grep -Fx -- "Injected pre-body" "$RSYSLOG_OUT_LOG" >/dev/null; then
	error_exit 1
fi

if [ -n "${OMMAIL_REPRO_CAPTURE:-}" ]; then
	cp "$RSYSLOG_OUT_LOG" "$OMMAIL_REPRO_CAPTURE"
fi

exit_test
