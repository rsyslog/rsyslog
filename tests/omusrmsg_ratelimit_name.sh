#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Test for omusrmsg ratelimit.name support (config-validation test).
# added 2025-07-10 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
ratelimit(name="omusrmsg_test_limit" interval="2" burst="5")

:msg, contains, "msgnum:" {
    action(type="omusrmsg" users="nouser"
           ratelimit.name="omusrmsg_test_limit")
}
'
startup
shutdown_when_empty
wait_shutdown
exit_test
