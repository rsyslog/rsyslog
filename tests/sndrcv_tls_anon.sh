#!/bin/bash
# rgerhards, 2011-04-04
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[sndrcv_tls_anon.sh\]: testing sending and receiving via TLS with anon auth
. $srcdir/sndrcv_drvr.sh sndrcv_tls_anon 25000
