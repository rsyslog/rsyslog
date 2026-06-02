#!/bin/bash
# Verify that a non-optional $IncludeConfig wildcard that matches no files logs
# a parser warning but does not prevent rsyslog from starting and processing
# messages. The warning is checked during config validation because include
# parsing happens before normal daemon action output is fully configured.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf "\$IncludeConfig ${srcdir}/testsuites/incltest.d/*.conf-not-there
"
add_conf '$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")'
export RS_REDIR=">${RSYSLOG_DYNNAME}.rsyslog.log 2>&1"
rsyslogd_config_check
unset RS_REDIR
content_check "IncludeConfig pattern '${srcdir}/testsuites/incltest.d/*.conf-not-there' did not match any files" \
	"${RSYSLOG_DYNNAME}.rsyslog.log"
startup
# 100 messages are enough - the question is if the include is read ;)
injectmsg 0 100
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 0 99
exit_test
