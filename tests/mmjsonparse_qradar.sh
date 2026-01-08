#!/bin/bash
# added 2025-10-01 by rgerhards
# check mmjsonparse with qradar generated json
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="subtree" subtree="$!qradar")

module(load="../plugins/mmjsonparse/.libs/mmjsonparse")

action(type="mmjsonparse" container="!qradar" cookie="<01> @cee:" useRawMsg="on" )
if $parsesuccess == "OK" then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
injectmsg_file $srcdir/testsuites/qradar_json
shutdown_when_empty
wait_shutdown 

# Validate semantic JSON equality between input and output
echo "Performing JSON semantic validation..."

# Try Python validation first
if [ -n "$PYTHON" ] && command -v "$PYTHON" >/dev/null 2>&1; then
    echo "Using Python for JSON validation..."
    if $PYTHON "$srcdir/validate_json_equality.py" $srcdir/testsuites/qradar_json $RSYSLOG_OUT_LOG "<01> @cee:"; then
        echo "✓ Python JSON validation passed!"
        validation_passed=true
    else
        echo "✗ Python JSON validation failed!"
        validation_passed=false
    fi
else
    echo "Python not available, trying shell validation..."
    if bash "$srcdir/validate_json_shell.sh" $srcdir/testsuites/qradar_json $RSYSLOG_OUT_LOG "<01> @cee:"; then
        echo "✓ Shell JSON validation passed!"
        validation_passed=true
    else
        echo "✗ Shell JSON validation failed!"
        validation_passed=false
    fi
fi

if [ "$validation_passed" != "true" ]; then
    echo "JSON validation failed! Showing actual output for manual review:"
    echo "============================================================"
    cat -n $RSYSLOG_OUT_LOG
    echo "============================================================"
    error_exit 1
fi

exit_test
