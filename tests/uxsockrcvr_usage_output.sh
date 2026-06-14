#!/bin/bash
# Test uxsockrcvr's standalone usage output when required options are missing.
# The oracle is the helper process output because no rsyslog instance is
# started; this covers the standalone testbench utility usage path from #3072.

. ${srcdir:=.}/diag.sh init

if ./uxsockrcvr > "$RSYSLOG_OUT_LOG" 2>&1; then
	echo "uxsockrcvr unexpectedly succeeded without required options"
	error_exit 1
fi
content_check 'usage: uxsockrcvr'
content_check '-s MUST be specified'

exit_test
