#!/bin/bash
# Pascal Withopf, 2017-07-25
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[sndrcv_tls_priorityString.sh\]: testing sending and receiving via TLS with anon auth
echo NOTE: When this test fails, it could be due to the priorityString being outdated!
. $srcdir/sndrcv_drvr.sh sndrcv_tls_priorityString 2500
