#!/bin/bash
# add 2017-10-30 by PascalWithopf, released under ASL 2.0
#tests for Segmentation Fault
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
#set $!r = $!var1!var3!var2;
'
startup
shutdown_when_empty
wait_shutdown

exit_test
