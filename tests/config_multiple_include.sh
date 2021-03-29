#!/bin/bash
# check nested include processing works correctly (and keeps proper order)
# added 2021-03-29 by Rainer Gerhards; Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5000
# Note: we need to pre-build the second level include file name because
# under "make distchek" we have a different environment and with the
# current rsyslog implemenation, we can only have a single environment
# variable in an `echo $VAR` block. This we cannot combine the include
# file name inside the config include without this trick.
export INCLUDE2="${srcdir}/testsuites/include-std2-omfile-action.conf"
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then {
	include(file="'${srcdir}'/testsuites/include-std1-omfile-actio*.conf")
	continue
}
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check
ls -l "$RSYSLOG2_OUT_LOG"
if [ -f "$RSYSLOG2_OUT_LOG" ]; then
	printf '\nERROR: file %s exists, but should have stopped by inner include\n' "$RSYSLOG2_OUT_LOG"
	printf 'This looks like a problem with order of include file processing.\n\n'
	error_exit 1
fi
exit_test
