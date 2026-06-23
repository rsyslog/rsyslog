#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
#
# Regression test for rsyslog/rsyslog#4348 in omhttp batch mode with
# batch.maxsize="1". A single HTTP 400 is still a permanent data failure for
# only that HTTP request payload; it must not leave the action in DATAFAIL state
# and block later good events. The mock server rejects message 00000002 and the
# oracle is that messages after it still arrive, with only message 2 missing.

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
		batch="on"
		batch.format="jsonarray"
		batch.maxsize="1"

		usehttps="off"
	)
'

startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown
omhttp_get_data $omhttp_server_lstnport my/endpoint jsonarray
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
