#!/bin/bash
# This test creates multiple symlinks (all watched by rsyslog via wildcard)
# chained to target files via additional symlinks and checks that all files
# are recorded with correct corresponding metadata (name of symlink
# matching configuration).
# This is part of the rsyslog testbench, released under ASL 2.0
. "${srcdir:=.}"/diag.sh init
. "$srcdir"/diag.sh check-inotify

# #define FILE_DELETE_DELAY 5 /* how many seconds to wait before finally deleting a gone file */
export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
export RSYSLOG_DEBUGLOG="log"
# Allow longer on ARM/slow CI for fd cleanup after symlink deletion
export TEST_TIMEOUT=60

# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
generate_conf
add_conf '
# comment out if you need more debug info:
global( debug.whitelist="on" debug.files=["imfile.c"]
  workDirectory="./'"$RSYSLOG_DYNNAME"'.work"
)
module(load="../plugins/imfile/.libs/imfile" mode="inotify")
input(type="imfile" File="./'"$RSYSLOG_DYNNAME"'.links/*.log" Tag="file:"
	Severity="error" Facility="local7" addMetadata="on")
template(name="outfmt" type="list") {
	constant(value="HEADER ")
	property(name="msg" format="json")
	constant(value=", filename: ")
	property(name="$!metadata!filename")
	constant(value=", fileoffset: ")
	property(name="$!metadata!fileoffset")
	constant(value="\n")
}
if $msg contains "msgnum:" then
	action( type="omfile" file="'"$RSYSLOG_DYNNAME.out/$RSYSLOG_OUT_LOG"'" template="outfmt")
'

mkdir "$RSYSLOG_DYNNAME".links "$RSYSLOG_DYNNAME".work "$RSYSLOG_DYNNAME".out

printf '\ncreating %s\n' "$RSYSLOG_DYNNAME".targets/container-1/logs/0.log
mkdir -p "$RSYSLOG_DYNNAME".targets/container-1/logs
./inputfilegen -m 1 >"$RSYSLOG_DYNNAME".targets/container-1/logs/0.log
ls -l "$RSYSLOG_DYNNAME".targets/container-1/logs/0.log
ln -sv "$PWD/$RSYSLOG_DYNNAME".targets/container-1/logs/0.log "$PWD/$RSYSLOG_DYNNAME".links/container-1.log
printf '%s generated link %s\n' "$(tb_timestamp)" "container-1"
ls -l "$RSYSLOG_DYNNAME".links/container-1.log

# Start rsyslog now
startup

PID=$(cat "$RSYSLOG_PIDBASE".pid)
echo "Rsyslog pid $RSYSLOG_PIDBASE.pid=$PID"
if [[ "$PID" == "" ]]; then
	error_exit 1
fi

echo "INFO: check files"
# wait until this files has been opened
check_fd_for_pid "$PID" exists "container-1/logs/0.log"
check_fd_for_pid "$PID" exists "container-1/logs"

echo "INFO: remove watched files"
rm -vr "$RSYSLOG_DYNNAME".targets/container-1
rm -v "$RSYSLOG_DYNNAME".links/container-1.log

until check_fd_for_pid "$PID" absent "container-1/logs (deleted)"; do
	if ((_wait_for_absent++ > TEST_TIMEOUT)); then
		error_exit 1
	fi
	echo "INFO: trigger fd unlinking"
	./inputfilegen -m 1 >"$RSYSLOG_DYNNAME".links/gogogo.log
	./msleep 1000
	rm -v "$RSYSLOG_DYNNAME".links/gogogo.log
	./msleep 10
done

shutdown_when_empty
wait_shutdown
exit_test
