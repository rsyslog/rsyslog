#!/bin/bash
# added 2018-04-06 by richm, released under ASL 2.0
#
# Note: on buildbot VMs (where there is no environment cleanup), the
# kubernetes test server may be kept running if the script aborts or
# is aborted (buildbot master failure!) for some reason. As such we
# execute it under "timeout" control, which ensure it always is
# terminated. It's not a 100% great method, but hopefully does the
# trick. -- rgerhards, 2018-07-21
#export RSYSLOG_DEBUG="debug"
export USE_VALGRIND="YES"
source ${srcdir:=.}/mmkubernetes-cache-expire.sh
