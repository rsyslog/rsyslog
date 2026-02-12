#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=200

VL_HOST="${VICTORIALOGS_HOST:-127.0.0.1}"
VL_PORT="${VICTORIALOGS_PORT:-29428}"
VL_BASE_URL="http://${VL_HOST}:${VL_PORT}"
COOKIE="vlogs-ci-$(date +%s)-$$"
VL_QUERY_OUT="${RSYSLOG_OUT_LOG}.victorialogs.query.out"

# Keep stream fields stable and low-cardinality.
VL_RESTPATH='insert/jsonline?_stream_fields=stream,host,app&_time_field=ts&_msg_field=msg'

for i in $(seq 1 40); do
	if curl -fsS "${VL_BASE_URL}/metrics" >/dev/null; then
		break
	fi
	sleep 0.5
	if [ "$i" -eq 40 ]; then
		echo "FAIL: VictoriaLogs is not reachable at ${VL_BASE_URL}"
		error_exit 1
	fi
done

generate_conf
add_conf '
module(load="../contrib/omhttp/.libs/omhttp")

template(name="tpl_victorialogs_jsonl" type="list" option.jsonf="on") {
	constant(outname="stream" value="rsyslog-ci" format="jsonf")
	property(outname="host" name="hostname" format="jsonf")
	property(outname="app" name="programname" format="jsonf")
	property(outname="ts" name="timegenerated" dateFormat="rfc3339" format="jsonf")
	constant(outname="testid" value="'$COOKIE'" format="jsonf")
	property(outname="msg" name="msg" format="jsonf")
}

if $msg contains "msgnum:" then
	action(
		name="omhttp_victorialogs_jsonline"
		type="omhttp"
		errorfile="'$RSYSLOG_DYNNAME/omhttp.victorialogs.error.log'"
		server="'$VL_HOST'"
		serverport="'$VL_PORT'"
		restpath="'$VL_RESTPATH'"
		httpcontenttype="application/stream+json"
		template="tpl_victorialogs_jsonl"
		batch="on"
		batch.format="newline"
		batch.maxsize="1000"
		batch.maxbytes="1048576"
		compress="on"
		usehttps="off"
	)
'

startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown

for i in $(seq 1 45); do
	curl -fsS --get "${VL_BASE_URL}/select/logsql/query" \
		--data-urlencode "query=msgnum" \
		--data-urlencode "limit=10000" > "${VL_QUERY_OUT}"

	line_count=$(grep -c "\"testid\":\"${COOKIE}\"" "${VL_QUERY_OUT}" || true)
	if [ "${line_count}" -ge "${NUMMESSAGES}" ]; then
		break
	fi
	sleep 1
	if [ "$i" -eq 45 ]; then
		echo "FAIL: timed out waiting for ${NUMMESSAGES} records in VictoriaLogs, got ${line_count}"
		error_exit 1
	fi
done

$PYTHON - <<'PY' "${VL_QUERY_OUT}" "${NUMMESSAGES}" "${COOKIE}"
import json
import re
import sys

path = sys.argv[1]
expected = int(sys.argv[2])
cookie = sys.argv[3]
pat = re.compile(r"msgnum:(\d+):")
seen = set()
count = 0

with open(path, "r", encoding="utf-8") as fh:
    for line in fh:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        if rec.get("testid") != cookie:
            continue
        count += 1
        msg = rec.get("_msg", "")
        m = pat.search(msg)
        if m:
            seen.add(int(m.group(1)))

if count < expected:
    raise SystemExit(f"expected at least {expected} records, got {count}")

if len(seen) != expected:
    raise SystemExit(f"expected {expected} unique msgnum values, got {len(seen)}")

if min(seen) != 0 or max(seen) != expected - 1:
    raise SystemExit(f"unexpected msgnum range: min={min(seen)} max={max(seen)} expected=0..{expected-1}")
PY

exit_test
