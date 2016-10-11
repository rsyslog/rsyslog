#!/bin/bash
# added 2013-12-10 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
# the first test is redundant, but we keep it when we later make the port
# configurable.
if [ `./omrelp_dflt_port` -lt 1024 ]; then
    if [ "$EUID" -ne 0 ]; then
	echo need to be root to run this test - skipping
        exit 77
    fi
fi
if [ `./omrelp_dflt_port` -ne 13515 ]; then
    echo this test needs configure option --enable-omrelp-default-port=13515 to work
    exit 77
fi
exit
. $srcdir/sndrcv_drvr.sh sndrcv_relp_dflt_pt 50000
