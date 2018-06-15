#!/bin/bash
# rgerhards, 2011-04-04
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[sndrcv_tls_ossl_anon_ipv6.sh\]: testing sending and receiving via TLS with anon auth using bare ipv6, no SNI
. $srcdir/diag.sh check-ipv6-available
. $srcdir/sndrcv_drvr.sh sndrcv_tls_ossl_anon_ipv6 10000
