#!/bin/bash
## @brief Validate GUID, IP address, and timestamp typing for mmsnareparse
## @description
## Exercises successful and failing GUID/IP/timestamp samples to ensure
## validated values are stored as typed JSON strings while malformed data
## follows the fallback path and records validation errors.
unset RSYSLOG_DYNNAME
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmsnareparse/.libs/mmsnareparse")

template(name="jsonout" type="list") {
    property(name="$!all-json")
    constant(value="\n")
}

action(type="mmsnareparse")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="jsonout")
'

startup
cat <<'MSG' > ${RSYSLOG_DYNNAME}.input
<13>1 2025-02-18T06:42:17.554128Z DC25-PREVIEW - - - - MSWinEventLog    1       Security        802301  Tue Feb 18 06:42:17 2024 624     Microsoft-Windows-Security-Auditing     N/A     N/A     Success Audit   DC25-PREVIEW    Logon           An account was successfully logged on.    Subject:   Security ID:  S-1-5-18   Account Name:  SYSTEM   Account Domain:  NT AUTHORITY   Logon ID:  0x3E7    Logon Information:   Logon Type:  2   Restricted Admin Mode: -   Virtual Account:  %%1843   Elevated Token:  %%1843    New Logon:   Security ID:  S-1-5-21-88997766-1122334455-6677889900-500   Account Name:  ADMIN-LAPS$   Account Domain:  FABRIKAM   Logon ID:  0x52F1A   Linked Logon ID:  0x0   Network Account Name: -   Network Account Domain: -   Logon GUID:  {5a8f0679-9b23-4cb7-a8c7-3d650c9b52ec}    Process Information:   Process ID:  0x66c   Process Name:  C:\Windows\System32\winlogon.exe    Network Information:   Workstation Name:  CORE25-01   Source Network Address: 192.168.50.12   Source Port:  59122    Detailed Authentication Information:   Logon Process:  User32   Authentication Package:  Negotiate   Transited Services: -   Package Name (NTLM only): -   Key Length:  0    Remote Credential Guard:  Enabled    -802301
<14>Sep 17 07:38:20 WIN-EXAMPLE MSWinEventLog    1       Security        9308    Tue Sep 17 07:38:20 2024        4616   Microsoft-Windows-Security-Auditing      N/A     N/A     Success Audit   WIN-EXAMPLE Security State Change           The system time was changed.    Subject:   Security ID:  S-1-5-19   Account Name:  LOCAL SERVICE   Account Domain:  NT AUTHORITY   Logon ID:  0x3E5    Process Information:   Process ID:  0x50c   Name:  C:\Windows\System32\svchost.exe    Previous Time:  2024-09-17T14:38:20.338436Z  New Time:  2024-09-17T14:38:20.500269Z    This event is generated when the system time is changed. It is normal for the Windows Time Service, which runs with System privilege, to change the system time on a regular basis. Other system time changes may be indicative of attempts to tamper with the computer.       323
<13>1 2025-02-18T06:55:01.000000Z DC25-PREVIEW - - - - MSWinEventLog    1       Security        999999  Tue Feb 18 06:55:01 2024 624     Microsoft-Windows-Security-Auditing     N/A     N/A     Success Audit   DC25-PREVIEW    Logon           An account was successfully logged on.    Subject:   Security ID:  S-1-5-18   Account Name:  SYSTEM   Account Domain:  NT AUTHORITY   Logon ID:  0x3E7    New Logon:   Security ID:  S-1-5-21-88997766-1122334455-6677889900-500   Account Name:  ADMIN-LAPS$   Account Domain:  FABRIKAM   Logon ID:  0x52F1A   Logon GUID:  not-a-guid    Network Information:   Workstation Name:  CORE25-01   Source Network Address: 999.999.999.999   Source Port:  59122    EventData:  Previous Time:  definitely-not-a-timestamp  New Time:  2024-99-99T25:61:61Z    Detailed Authentication Information:   Logon Process:  User32   Authentication Package:  Negotiate    -999999
MSG
injectmsg_file ${RSYSLOG_DYNNAME}.input

shutdown_when_empty
wait_shutdown

python3 - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as fh:
    docs = [json.loads(line) for line in fh if line.strip()]

assert len(docs) == 3, f"expected three events, got {len(docs)}"
first = docs[0]["win"]
second = docs[1]["win"]
third = docs[2]["win"]

logon_guid = first["NewLogon"]["LogonGUID"]
assert isinstance(logon_guid, str), "LogonGUID should be a string"
assert logon_guid == "{5a8f0679-9b23-4cb7-a8c7-3d650c9b52ec}"

source_ip = first["Network"]["SourceNetworkAddress"]
assert isinstance(source_ip, str), "SourceNetworkAddress should be a string"
assert source_ip == "192.168.50.12"

previous_time = second["EventData"]["PreviousTime"]
new_time = second["EventData"]["NewTime"]
for value in (previous_time, new_time):
    assert isinstance(value, str), "timestamp values should remain strings"
assert previous_time == "2024-09-17T14:38:20.338436Z"
assert new_time == "2024-09-17T14:38:20.500269Z"

bad_guid = third["NewLogon"]["LogonGUID"]
assert isinstance(bad_guid, str), "fallback GUID should remain a string"
assert bad_guid == "not-a-guid"

bad_ip = third["Network"]["SourceNetworkAddress"]
assert isinstance(bad_ip, str), "fallback IP should remain a string"
assert bad_ip == "999.999.999.999"

bad_prev = third["EventData"]["PreviousTime"]
bad_new = third["EventData"]["NewTime"]
for value in (bad_prev, bad_new):
    assert isinstance(value, str), "fallback timestamps should remain strings"
assert bad_prev == "definitely-not-a-timestamp"
assert bad_new == "2024-99-99T25:61:61Z"

errors = third.get("Validation", {}).get("Errors", [])
joined = "\n".join(errors)
assert "LogonGUID" in joined, "expected validation error for invalid GUID"
assert "SourceNetworkAddress" in joined, "expected validation error for invalid IP"
assert "PreviousTime" in joined, "expected validation error for invalid timestamp"
PY

exit_test
