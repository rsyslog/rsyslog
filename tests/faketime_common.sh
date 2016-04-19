#!/bin/bash
# addd 2016-03-11 by Thomas D., released under ASL 2.0
# Several tests make use of faketime. They all need to know when
# faketime is missing or the system isn't year-2038 complaint.
# This script can be sourced to prevent duplicated code.

faketime_testtime=$(LD_PRELOAD=libfaketime.so FAKETIME="1991-08-25 20:57:08" TZ=GMT date +%s 2>/dev/null)
if [ ${faketime_testtime} -ne 683153828 ] ; then
    echo "libfaketime.so missing, skipping test"
    exit 77
fi

# GMT-1 (POSIX TIME) is GMT+1 in "Human Time"
faketime_testtime=$(LD_PRELOAD=libfaketime.so FAKETIME="2040-01-01 16:00:00" TZ=GMT-1 date +%s 2>/dev/null)
if [ ${faketime_testtime} -eq -1 ]; then
    # System isn't year-2038 compatible
    RSYSLOG_TESTBENCH_Y2K38_INCOMPATIBLE="yes"
fi

export LD_PRELOAD=libfaketime.so

rsyslog_testbench_require_y2k38_support() {
    if [ -n "${RSYSLOG_TESTBENCH_Y2K38_INCOMPATIBLE}" ]; then
        echo "Skipping further tests because system doesn't support year 2038 ..."
        . $srcdir/diag.sh exit
        exit 0
    fi
}
