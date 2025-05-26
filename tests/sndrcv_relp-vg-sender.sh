#!/bin/bash
# added 2022-06-21 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

# CHECK VALGRIND MINIMUM VERSION | MIN 3.14.0
VALGRINDVER=$(valgrind --version)
VALGRINDVERMAJOR=$(echo $VALGRINDVER | cut -d'-' -f2 | cut -d'.' -f1)
VALGRINDVERMINOR=$(echo $VALGRINDVER | cut -d'-' -f2 | cut -d'.' -f2)
if [ "$VALGRINDVERMAJOR" -lt 3 ] || { [ "$VALGRINDVERMAJOR" -eq 3 ] && [ "$VALGRINDVERMINOR" -lt 15 ]; }; then
        printf 'This test does NOT work with versions below valgrind-3.15.0 (missing --keep-debuginfo) - Installed valgrind version is '
        printf $VALGRINDVER
        printf '\n'
        exit 77
fi

export NUMMESSAGES=5000
export RS_TEST_VALGRIND_EXTRA_OPTS="--keep-debuginfo=yes --leak-check=full"
########## receiver ##########
export RSYSLOG_DEBUG="debug nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
export PORT_RCVR="$(get_free_port)"
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="'$PORT_RCVR'")

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
printf "#### RECEIVER STARTED\n\n"

########## sender ##########
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender.debuglog"
generate_conf 2
add_conf '
module(load="../plugins/omrelp/.libs/omrelp")

action(type="omrelp" name="omrelp" target="127.0.0.1" port="'$PORT_RCVR'")
' 2

startup_vg 2
printf "#### SENDER STARTED\n\n"

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg2 0 $NUMMESSAGES

printf "#### SENDER SHUTDOWN\n\n"
shutdown_when_empty 2
wait_shutdown_vg 2
check_exit_vg 2

printf "#### RECEIVER SHUTDOWN\n\n"
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown
seq_check
exit_test
