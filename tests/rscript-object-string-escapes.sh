#!/bin/bash
# Verify that RainerScript object/action parameter strings and expression
# strings accept the same documented byte escapes that the unescape layer
# decodes. The oracle is the configured omfile output after synchronized
# shutdown, proving hex and octal forms reached the normal template/action path
# as the intended bytes.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
set $.expr_hex_upper = "\x5A";
set $.expr_hex_upper_digits = "\x4A";

template(name="outfmt" type="list") {
	constant(value="hex_lower=\x41\n")
	constant(value="hex_upper=\x5a\n")
	constant(value="hex_upper_digits=\x5A\n")
	constant(value="expr_hex_upper=")
	property(name="$.expr_hex_upper")
	constant(value="\n")
	constant(value="expr_hex_upper_digits=")
	property(name="$.expr_hex_upper_digits")
	constant(value="\n")
	constant(value="octal=\101\n")
	constant(value="literal=\\\\x41\n")
}

:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

cat > "$RSYSLOG_OUT_LOG.expect" <<'EOF'
hex_lower=A
hex_upper=Z
hex_upper_digits=Z
expr_hex_upper=Z
expr_hex_upper_digits=J
octal=A
literal=\x41
EOF

cmp_exact_file "$RSYSLOG_OUT_LOG.expect" "$RSYSLOG_OUT_LOG"
exit_test
