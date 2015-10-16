#!/bin/bash
# added 2014-11-05 by Rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[sndrcv_udp_nonstdpt_v6.sh\]: testing sending and receiving via udp
. $srcdir/sndrcv_drvr.sh sndrcv_udp_nonstdpt_v6 500
