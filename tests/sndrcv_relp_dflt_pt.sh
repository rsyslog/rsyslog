#!/bin/bash
# added 2013-12-10 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
if [ `./omrelp_dflt_port` -lt 1024 ]; then
    if [ "$EUID" -ne 0 ]; then
	echo need to be root to run this test - skipping
        exit 77
    fi
fi
. $srcdir/sndrcv_drvr.sh sndrcv_relp_dflt_pt 50000
