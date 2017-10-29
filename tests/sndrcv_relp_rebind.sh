#!/bin/bash
# added 2017-09-29 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ====================================================================================
echo \[sndrcv_relp_rebind.sh\]: testing sending and receiving via relp w/ rebind interval
. $srcdir/sndrcv_drvr.sh sndrcv_relp_rebind 1000
