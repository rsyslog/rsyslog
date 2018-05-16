#!/bin/bash
# rgerhards, 2011-04-04
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[sndrcv_tls_ossl_anon_ipv4.sh\]: testing sending and receiving via TLS with anon auth using bare ipv4, no SNI
. $srcdir/sndrcv_drvr.sh sndrcv_tls_ossl_anon_ipv4 10000
