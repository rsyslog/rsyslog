#!/bin/bash
# addd 2017-03-06 by RGerhards, released under ASL 2.0

# Note: we need to inject a somewhat larger nubmer of messages in order
# to ensure that we receive some messages in the actual output file,
# as batching can (validly) cause a larger loss in the non-writable
# file

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
global(umask="0077")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" {
	action(type="omfile" template="outfmt" file="rsyslog.out.log")
}
'
. $srcdir/diag.sh startup
$srcdir/diag.sh injectmsg 0 1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

if [ `ls -l rsyslog.o*|$RS_HEADCMD -c 10 ` != "-rw-------" ]; then
  echo "invalid file permission (umask), rsyslog.out.log has:"
  ls -l rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;
. $srcdir/diag.sh exit
