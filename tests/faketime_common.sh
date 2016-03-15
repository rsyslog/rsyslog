#!/bin/bash
# addd 2016-03-11 by Thomas D., released under ASL 2.0
# Several tests make use of faketime. They all need to know when
# faketime is missing or the system isn't year-2038 complaint.
# This script can be sourced to prevent duplicated code.

if ! hash faketime 2>/dev/null ; then
    echo "faketime command missing, skipping test"
    exit 77
fi

export TZ=UTC+01:00

faketime -f '2016-03-11 16:00:00' date 1>/dev/null 2>&1
if [ $? -ne 0 ]; then
    # Safe-guard -- should never happen!
    echo "faketime command not working as expected. Check faketime binary in path!"
    exit 1
fi

faketime '2040-01-01 16:00:00' date 1>/dev/null 2>&1
if [ $? -ne 0 ]; then
    # System isn't year-2038 compatible
    RSYSLOG_TESTBENCH_Y2K38_INCOMPATIBLE="yes"
fi


rsyslog_testbench_require_y2k38_support() {
  if [ -n "${RSYSLOG_TESTBENCH_Y2K38_INCOMPATIBLE}" ]; then
    echo "Skipping further tests because system doesn't support year 2038 ..."
    . $srcdir/diag.sh exit
  fi
}
