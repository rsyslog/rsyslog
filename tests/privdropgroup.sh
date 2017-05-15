#!/bin/bash
# addd 2016-03-24 by RGerhards, released under ASL 2.0

uname
if [ `uname` = "SunOS" ] ; then
   echo "Solaris: FIX ME"
   exit 77
fi

. $srcdir/privdrop_common.sh
rsyslog_testbench_setup_testuser

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
global(privdrop.group.keepsupplemental="on")
template(name="outfmt" type="list") {
	property(name="msg" compressSpace="on")
	constant(value="\n")
}
action(type="omfile" template="outfmt" file="rsyslog.out.log")
'
. $srcdir/diag.sh add-conf "\$PrivDropToGroup ${TESTBENCH_TESTUSER[groupname]}"
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
grep "groupid.*${TESTBENCH_TESTUSER[gid]}" < rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "message indicating drop to group \"${TESTBENCH_TESTUSER[groupname]}\" (#${TESTBENCH_TESTUSER[gid]}) is missing:"
  cat rsyslog.out.log
  exit 1
fi;

. $srcdir/diag.sh exit
