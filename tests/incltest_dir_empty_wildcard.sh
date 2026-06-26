#!/bin/bash
# Verify that a non-optional $IncludeConfig wildcard that matches no files is
# accepted silently. The oracle is successful config validation and runtime
# message processing, with no include no-match, warning, or error diagnostic in
# the captured rsyslogd output.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf "\$IncludeConfig ${srcdir}/testsuites/incltest.d/*.conf-not-there
"
add_conf 'global(compatibility.defaults.secure="strict")
$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")'
DIAG_LOG="${RSYSLOG_DYNNAME}.rsyslog.log"
: > "$DIAG_LOG"
export RS_REDIR=">>\"$DIAG_LOG\" 2>&1"
rsyslogd_config_check
startup
unset RS_REDIR
# 100 messages are enough - the question is if the include is read ;)
injectmsg 0 100
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
check_not_present "IncludeConfig pattern" "$DIAG_LOG"
check_not_present "[Ww][Aa][Rr][Nn]" "$DIAG_LOG"
check_not_present "[Ee][Rr][Rr][Oo][Rr]" "$DIAG_LOG"
seq_check 0 99
exit_test
