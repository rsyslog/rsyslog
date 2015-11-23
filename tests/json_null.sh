#!/bin/bash
# added 2015-11-17 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
# Note: the aim of this test is to test against misadressing, so we do
# not actually check the output
echo ===============================================================================
echo \[json_null.sh\]: test for json containung \"null\" value
. $srcdir/diag.sh init
. $srcdir/diag.sh startup-vg json_null.conf
. $srcdir/diag.sh tcpflood -m 1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 test: @cee: { \\\"nope\\\": null }\""
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh exit
