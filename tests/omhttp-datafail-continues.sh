#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
#
# Regression test for rsyslog/rsyslog#4348: a permanent omhttp data
# failure such as HTTP 400 must drop only the rejected event and must not
# leave the action in DATAFAIL state so later good events stop flowing.
# The mock server rejects message 00000002 with HTTP 400 and accepts all
# other payloads. The oracle is that messages after the rejected payload
# still arrive, with only message 2 missing from the accepted sequence.

# shellcheck disable=SC2034,SC2154

. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=10

omhttp_start_server 0 --fail-msgnum-400 00000002

generate_conf
add_conf '
module(load="../contrib/omhttp/.libs/omhttp")

template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

if $msg contains "msgnum:" then
	action(
		name="action_omhttp"
		type="omhttp"
		errorfile="'$RSYSLOG_DYNNAME/omhttp.error.log'"
		template="tpl"

		server="localhost"
		serverport="'$omhttp_server_lstnport'"
		restpath="my/endpoint"
		batch="off"

		usehttps="off"
	)
'

startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown
omhttp_get_data $omhttp_server_lstnport my/endpoint
omhttp_stop_server
EXPECTED='00000000
00000001
00000003
00000004
00000005
00000006
00000007
00000008
00000009'
cmp_exact
exit_test
