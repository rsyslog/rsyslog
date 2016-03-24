#!/bin/bash
# We assume that a group equally named to the user exists
# addd 2016-03-24 by RGerhards, released under ASL 2.0
. $srcdir/diag.sh init
USER_GID=`id -g $USER`
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
global(privdrop.group.keepsupplemental="on")
template(name="outfmt" type="list") {
	property(name="msg" compressSpace="on")
	constant(value="\n")
}
action(type="omfile" template="outfmt" file="rsyslog.out.log")
'
. $srcdir/diag.sh add-conf "\$PrivDropToGroupID $USER_GID"

. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
grep "groupid.*$USER_GID" < rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "message indicating drop to gid $USER_GID is missing:"
  cat rsyslog.out.log
  exit 1
fi;

. $srcdir/diag.sh exit
