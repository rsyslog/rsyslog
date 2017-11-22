#!/bin/bash
# rgerhards, 2011-04-04
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[sndrcv_tls_anon_hostname.sh\]: testing sending and receiving via TLS with anon auth using hostname SNI
. $srcdir/sndrcv_drvr.sh sndrcv_tls_anon_hostname 25000
