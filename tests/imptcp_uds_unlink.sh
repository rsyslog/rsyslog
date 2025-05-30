#!/bin/bash
. ${srcdir:=.}/diag.sh init
# First make sure we don't unlink if not asked to
rm -f "$srcdir/testbench_socket"
echo "nope" > "$srcdir/testbench_socket"

generate_conf
add_conf '
global(MaxMessageSize="124k")

module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" path="'$RSYSLOG_DYNNAME'-testbench_socket" unlink="on" filecreatemode="0600")
input(type="imptcp" path="'$RSYSLOG_DYNNAME'-testbench_socket2" unlink="on" filecreatemode="0666")

template(name="outfmt" type="string" string="%msg:%\n")
*.notice	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
content_check "imptcp: error while binding unix socket: Address already in use"
exit_test

# Now make sure we unlink if asked to
echo "ok" > "$srcdir/testbench_socket"

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(MaxMessageSize="124k")

module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" path="'$RSYSLOG_DYNNAME'-testbench_socket" unlink="on")

template(name="outfmt" type="string" string="%msg:%\n")
*.notice	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup

logger -Tu "$srcdir/testbench_socket" "hello from imptcp uds"

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
content_check "hello from imptcp uds"
exit_test
