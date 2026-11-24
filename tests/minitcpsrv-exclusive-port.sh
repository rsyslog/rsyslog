#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# An offline reservation must remain unavailable and prevent a competing
# SO_REUSEADDR socket from becoming a listener. The helper's port file proves
# bind completed; the unreleased gate keeps it offline without timing guesses.
# Invalid exclusive-mode combinations are rejected; default -w still transitions
# to a working listener when released. A one-second connect deadline bounds
# the negative probe: Linux refuses immediately, while macOS may time out.
# Exclusivity is independently proved first by the competing-listener check.
. ${srcdir:=.}/diag.sh init

generate_conf
# Invalid combinations must be rejected by option validation before socket setup.
if ./minitcpsrv -E -t 127.0.0.1 -p 0 -f "$RSYSLOG_DYNNAME.invalid" \
    >"$RSYSLOG_DYNNAME.no-gate" 2>&1; then
    error_exit 1 "exclusive mode accepted without an offline gate"
fi
content_check "requires -w" "$RSYSLOG_DYNNAME.no-gate"
if ./minitcpsrv -E -R -w "$RSYSLOG_DYNNAME.invalid-release" \
    -t 127.0.0.1 -p 0 -f "$RSYSLOG_DYNNAME.invalid" \
    >"$RSYSLOG_DYNNAME.reuseport" 2>&1; then
    error_exit 1 "exclusive mode accepted with port reuse"
fi
content_check "cannot be combined with -R" "$RSYSLOG_DYNNAME.reuseport"

# Existing default -w behavior must still permit transition to a real listener.
start_minitcpsrvr "$RSYSLOG_DYNNAME.default.log" 1 \
    "$RSYSLOG_DYNNAME.default-release" "$RSYSLOG_DYNNAME.default-ready"
: > "$RSYSLOG_DYNNAME.default-release"
wait_file_exists "$RSYSLOG_DYNNAME.default-ready"
"$PYTHON" - "$MINITCPSRVR_PORT1" <<'PYDEFAULT'
import socket
import sys
client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect(('127.0.0.1', int(sys.argv[1])))
client.sendall(b'default-mode\n')
client.close()
PYDEFAULT
[ "$?" -eq 0 ] || error_exit 1
wait_file_lines "$RSYSLOG_DYNNAME.default.log" 1
content_check "default-mode" "$RSYSLOG_DYNNAME.default.log"

MINITCPSRV_EXTRA_OPTS="-E" start_minitcpsrvr "$RSYSLOG_DYNNAME.unused.log" 2 \
    "$RSYSLOG_DYNNAME.listen-release"

"$PYTHON" - "$MINITCPSRVR_PORT2" <<'PY'
from __future__ import print_function
import errno
import socket
import sys

address = ('127.0.0.1', int(sys.argv[1]))
competitor = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
competitor.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try:
    competitor.bind(address)
    competitor.listen(1)
except socket.error as error:
    if error.errno != errno.EADDRINUSE:
        raise
    print('competing listener blocked with EADDRINUSE')
else:
    raise SystemExit('FAIL: competing SO_REUSEADDR socket became a listener')
finally:
    competitor.close()

probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
probe.settimeout(1)
try:
    probe.connect(address)
except socket.timeout:
    pass
except socket.error as error:
    if error.errno not in (errno.ECONNREFUSED, errno.ETIMEDOUT):
        raise
else:
    raise SystemExit('offline holder unexpectedly accepted a connection')
finally:
    probe.close()

PY
[ "$?" -eq 0 ] || error_exit 1
exit_test
