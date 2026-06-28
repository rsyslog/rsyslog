#!/usr/bin/env bash
# Verify the tcpflood wrapper records a proper-termination marker for normal
# helper completion. The oracle is the marker itself: UDP transport avoids a
# receiver dependency, and a marker write failure must make tcpflood fail.
. ${srcdir:=.}/diag.sh init

TCPFLOOD_PORT=9 tcpflood -Tudp -m1 -s
tcpflood_marker="${RSYSLOG_DYNNAME}.tcpflood.1.proper-termination"
if ! tcpflood_marker_is_valid "$tcpflood_marker"; then
	echo "FAIL: tcpflood marker missing or invalid"
	print_tcpflood_marker_file "$tcpflood_marker"
	error_exit 1
fi
content_check "pid=" "$tcpflood_marker"
content_check "version=" "$tcpflood_marker"

if ./tcpflood -q / -p9 -Tudp -m1 -s; then
	echo "FAIL: tcpflood marker write failure was not reported"
	error_exit 1
fi

symlink_target="${RSYSLOG_DYNNAME}.tcpflood.symlink-target"
symlink_marker="${RSYSLOG_DYNNAME}.tcpflood.symlink-marker"
echo "unchanged" > "$symlink_target"
ln -s "$symlink_target" "$symlink_marker"
if ./tcpflood -q "$symlink_marker" -p9 -Tudp -m1 -s; then
	echo "FAIL: tcpflood followed symlink marker path"
	error_exit 1
fi
content_check "unchanged" "$symlink_target"

exit_test
