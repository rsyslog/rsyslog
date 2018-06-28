#!/bin/bash
# added 2013-12-10 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[sndrcv_relp_tls_prio.sh\]: testing sending and receiving via relp with TLS enabled and priority string set
. $srcdir/sndrcv_drvr.sh sndrcv_relp_tls_prio 50000
