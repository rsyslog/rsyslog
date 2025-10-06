#!/bin/bash
## @file tests/mmsnareparse-enhanced-validation.sh
## @brief Validate enhanced parsing, validation, and stats for mmsnareparse.

unset RSYSLOG_DYNNAME
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmsnareparse/.libs/mmsnareparse")

template(name="validation_test_json" type="list" option.jsonf="on") {
    property(outname="eventid" name="$!win!Event!EventID" format="jsonf")
    property(outname="validation_errors" name="$!win!Validation!Errors" format="jsonf")
    property(outname="parsing_stats" name="$!win!Stats!ParsingStats" format="jsonf")
    property(outname="event_json" name="$!win" format="jsonf")
}

input(type="imtcp" port="'$TCPFLOOD_PORT'")
action(type="mmsnareparse"
       rootpath="!win"
       validation_mode="strict"
       debugjson="on"
       template="validation_test_json")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="validation_test_json")
'

startup
tcpflood -m1 -M "\"<14>Jun 10 11:30:45 LAB-LOGON105 MSWinEventLog\t1\tSecurity\t10105\tMon Jun 10 11:30:45 2024\t4624\tMicrosoft-Windows-Security-Auditing\tN/A\tN/A\tSuccess Audit\tLAB-LOGON105\tLogon\t\tAn account was successfully logged on. Subject: Security ID: SYSTEM Account Name: HOST-007\\$ Account Domain: WORKGROUP Logon ID: 0x3E7 Logon Information: Logon Type: 7 Restricted Admin Mode: - Remote Credential Guard: - Virtual Account: No Elevated Token: No Impersonation Level: Impersonation New Logon: Security ID: CLOUDDOMAIN\\User009 Account Name: user010@example.com Account Domain: CLOUDDOMAIN Logon ID: 0xFD5113F Linked Logon ID: 0xFD5112A Network Account Name: - Network Account Domain: - Logon GUID: {00000000-0000-0000-0000-000000000000} Process Information: Process ID: 0x30c Process Name: C:\\Windows\\System32\\lsass.exe Network Information: Workstation Name: HOST-007 Source Network Address: - Source Port: - Detailed Authentication Information: Logon Process: Negotiat Authentication Package: Negotiate Transited Services: - Package Name (NTLM only): - Key Length: 0\t1\""
shutdown_when_empty
wait_shutdown

python3 - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = []

try:
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            data.append(json.loads(line))
        except json.JSONDecodeError as exc:
            raise SystemExit(f"failed to decode JSON line: {line}\n{exc}")
except FileNotFoundError as exc:
    raise SystemExit(f"output log not found: {exc}")

event = next((entry for entry in data if entry.get("eventid")), None)
if event is None:
    raise SystemExit("no event record with an eventid was captured")

try:
    stats = json.loads(event["parsing_stats"])
except (KeyError, json.JSONDecodeError) as exc:
    raise SystemExit(f"unable to parse parsing_stats payload: {exc}")

try:
    errors = json.loads(event["validation_errors"])
except (KeyError, json.JSONDecodeError) as exc:
    raise SystemExit(f"unable to parse validation_errors payload: {exc}")

if event["eventid"] != "4624":
    raise SystemExit(f"unexpected eventid: {event['eventid']}")

if errors != []:
    raise SystemExit(f"expected no validation errors, got: {errors}")

expected_stats = {"total_fields": 25, "successful_parses": 25, "failed_parses": 0}
if stats != expected_stats:
    raise SystemExit(f"unexpected parsing stats: {stats}")

try:
    event_root = json.loads(event["event_json"])
except (KeyError, json.JSONDecodeError) as exc:
    raise SystemExit(f"unable to parse event_json payload: {exc}")

logon = event_root.get("Logon", {})
new_logon = event_root.get("NewLogon", {})
network = event_root.get("Network", {})
auth = event_root.get("Authentication", {})

placeholder_checks = [
    (logon, "RemoteCredentialGuard", "Logon"),
    (new_logon, "NetworkAccountName", "NewLogon"),
    (new_logon, "NetworkAccountDomain", "NewLogon"),
    (network, "SourceNetworkAddress", "Network"),
    (network, "SourcePort", "Network"),
    (auth, "TransitedServices", "Authentication"),
    (auth, "PackageName", "Authentication"),
]

for container_obj, field_name, container_name in placeholder_checks:
    if field_name in container_obj:
        raise SystemExit(
            f"placeholder field {container_name}.{field_name} should be absent but was present: {container_obj[field_name]}"
        )
PY

exit_test
