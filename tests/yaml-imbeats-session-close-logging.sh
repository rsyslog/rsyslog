#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# YAML counterpart of imbeats-session-close-logging.sh: verify that the
# session-close diagnostics also work when the input is configured through the
# YAML frontend, including the input "name", "maxWindowSize" and "idleTimeout"
# parameters whose values the message text reports.
#
# Setup: the module, the input and its ruleset come from an included YAML
# config; rsyslog's own messages are written to a file by a top-level action in
# the RainerScript part, because the YAML frontend cannot add actions to
# RSYSLOG_DefaultRuleset (that name may not be declared explicitly). The
# listener uses name=closelogyaml, maxWindowSize=4 and idleTimeout=2.
# Stimulus and oracle (read from the internal-message file after a
# synchronized shutdown):
#  1. one window announcing 5 events (> maxWindowSize) must be closed by the
#     server without an ACK and logged as "window size 5 exceeds
#     maxWindowSize 4", naming peer 127.0.0.1 and the listener port;
#  2. one connection that sends nothing must be closed after idleTimeout and
#     logged as "idle timeout (idleTimeout 2 s)".
# Each client waits for that close with a bounded receive (15 s). The bound is
# only hang protection: a connection that stays silently open fails the test
# because the expected message would then be missing. The rate limiter is not
# exercised here; imbeats-session-close-logging.sh covers it.
. ${srcdir:=.}/diag.sh init
require_yaml_support

cat > "${RSYSLOG_DYNNAME}.yaml" <<YAML_EOF
version: 2
modules:
  - load: "../plugins/imbeats/.libs/imbeats"
templates:
  - name: closelogfmt
    type: string
    string: "%msg%\\n"
inputs:
  - type: imbeats
    port: "0"
    listenPortFileName: "${RSYSLOG_DYNNAME}.imbeats.port"
    ruleset: main
    name: closelogyaml
    maxWindowSize: 4
    idleTimeout: 2
rulesets:
  - name: main
    actions:
      - type: omfile
        file: "${RSYSLOG_OUT_LOG}"
        template: closelogfmt
YAML_EOF

generate_conf
add_conf 'include(file="'${RSYSLOG_DYNNAME}'.yaml")
# rsyslog internal messages land in the default ruleset
action(type="omfile" file="'$RSYSLOG_DYNNAME'.internal.log" template="closelogfmt")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

if ! $PYTHON - "$RS_PORT" <<'PY'
import socket
import struct
import sys

port = int(sys.argv[1])


def wait_closed(sock, what, limit=15):
    """Return once the server closed the connection; fail on data or timeout."""
    sock.settimeout(limit)
    try:
        data = sock.recv(6)
    except (ConnectionResetError, BrokenPipeError):
        return
    except (TimeoutError, socket.timeout):
        raise SystemExit(f"{what}: server did not close the connection")
    if data:
        raise SystemExit(f"{what}: unexpected data {data.hex()}")


# 1. window announcing more events than maxWindowSize
with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
    try:
        sock.sendall(b"2W" + struct.pack(">I", 5))
    except (ConnectionResetError, BrokenPipeError):
        sys.exit(0)
    wait_closed(sock, "oversized window")

# 2. idle connection, closed by idleTimeout=2
with socket.create_connection(("127.0.0.1", port), timeout=5) as idle:
    wait_closed(idle, "idle session")
PY
then
	error_exit 1
fi

shutdown_when_empty
wait_shutdown

LOG="$RSYSLOG_DYNNAME.internal.log"

content_check "imbeats: closing session from 127.0.0.1:" "$LOG"
content_check "on port $RS_PORT (input name 'closelogyaml'): window size 5 exceeds maxWindowSize 4" "$LOG"
content_check "on port $RS_PORT (input name 'closelogyaml'): idle timeout (idleTimeout 2 s)" "$LOG"

exit_test
