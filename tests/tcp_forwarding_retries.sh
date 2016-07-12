#!/bin/bash
# added 2016-06-21 by RGerhards, released under ASL 2.0

messages=20000 # how many messages to inject?
# Note: we need to inject a somewhat larger nubmer of messages in order
# to ensure that we receive some messages in the actual output file,
# as batching can (validly) cause a larger loss in the non-writable
# file

. $srcdir/diag.sh init

# we start a small receiver process
./minitcpsrv -t127.0.0.1 -p13514 -frsyslog.out.log -s4 &
BGPROCESS=$!
echo background minitcpsrvr process id is $BGPROCESS

. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" {
	action(type="omfwd"
	       target="127.0.0.1" port="13514" protocol="TCP"
	       action.resumeRetryCount="10"
	       template="outfmt")
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh injectmsg 0 $messages
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

# note: minitcpsrvr shuts down automatically if the connection is closed, but
# we still try to kill it in case the test did not connect to it! Note that we
# do not need an extra wait, as the rsyslog shutdown process should have taken
# far long enough.
echo wating on background process
#kill $BGPROCESS &> /dev/null
wait $BGPROCESS

. $srcdir/diag.sh seq-check 0 $(($messages-1))
. $srcdir/diag.sh exit
