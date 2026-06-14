#!/bin/bash
# Covers the chkseq testbench helper usage path. The oracle is the diagnostic
# text for an invalid option, proving the helper still prints actionable usage
# output instead of failing silently.

. ${srcdir:=.}/diag.sh init

if ./chkseq -Z > "$RSYSLOG_OUT_LOG" 2>&1; then
	echo "chkseq unexpectedly succeeded with an invalid option"
	error_exit 1
fi
content_check 'Invalid call of chkseq'
content_check 'Usage: chkseq'

exit_test
