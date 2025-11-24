#!/bin/bash
# Validate mmsnareparse parsing with trailing extra-data section truncation using regex.
# This test verifies regex support for dynamic numeric prefixes in trailing custom data.
unset RSYSLOG_DYNNAME
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmsnareparse/.libs/mmsnareparse")

template(name="outfmt" type="list") {
    property(name="$!win!Event!EventID")
    constant(value=",")
    property(name="$!win!Event!Channel")
    constant(value=",")
    property(name="$!win!EventData!EventType")
    constant(value=",")
    property(name="$!win!EventData!TargetObject")
    constant(value=",")
    property(name="$!win!EventData!User")
    constant(value=",")
    property(name="$!extradata_section")
    constant(value="\n")
}

action(type="mmsnareparse"
       definition.file="../plugins/mmsnareparse/sysmon_definitions.json"
       ignoreTrailingPattern.regex="^[0-9]+[[:space:]]+custom_section:")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

startup

# Test case 1: Standard number prefix (3385599)
cat <<'MSG' > ${RSYSLOG_DYNNAME}.input1
<14>Mar 22 08:47:23 testhost MSWinEventLog	1	Microsoft-Windows-Sysmon/Operational	20977	Mon Mar 22 08:47:23 2025	13	Windows	SYSTEM	User	SetValue	testhost	Registry value set (rule: RegistryEvent)		Registry value set:  RuleName: Default RegistryEvent  EventType: SetValue  UtcTime: 2025-03-22 08:47:23.284  ProcessGuid: {fd4d0da6-d589-6916-eb03-000000000000}  ProcessId: 4  Image: System  TargetObject: HKLM\System\CurrentControlSet\Services\TestService\ImagePath  Details: "C:\Program Files\TestAgent\TestService.exe"  User: NT AUTHORITY\SYSTEM	3385599 custom_section: fromhost-ip=192.168.45.217
MSG

# Test case 2: Different number (12345)
cat <<'MSG' > ${RSYSLOG_DYNNAME}.input2
<14>Mar 22 08:47:24 testhost MSWinEventLog	1	Microsoft-Windows-Sysmon/Operational	20978	Mon Mar 22 08:47:24 2025	13	Windows	SYSTEM	User	SetValue	testhost	Registry value set (rule: RegistryEvent)		Registry value set:  RuleName: Default RegistryEvent  EventType: SetValue  UtcTime: 2025-03-22 08:47:24.284  ProcessGuid: {fd4d0da6-d589-6916-eb03-000000000001}  ProcessId: 4  Image: System  TargetObject: HKLM\System\CurrentControlSet\Services\TestService\ImagePath  Details: "C:\Program Files\TestAgent\TestService.exe"  User: NT AUTHORITY\SYSTEM	12345 custom_section: fromhost-ip=192.168.45.218
MSG

# Test case 3: Single digit (9)
cat <<'MSG' > ${RSYSLOG_DYNNAME}.input3
<14>Mar 22 08:47:25 testhost MSWinEventLog	1	Microsoft-Windows-Sysmon/Operational	20979	Mon Mar 22 08:47:25 2025	13	Windows	SYSTEM	User	SetValue	testhost	Registry value set (rule: RegistryEvent)		Registry value set:  RuleName: Default RegistryEvent  EventType: SetValue  UtcTime: 2025-03-22 08:47:25.284  ProcessGuid: {fd4d0da6-d589-6916-eb03-000000000002}  ProcessId: 4  Image: System  TargetObject: HKLM\System\CurrentControlSet\Services\TestService\ImagePath  Details: "C:\Program Files\TestAgent\TestService.exe"  User: NT AUTHORITY\SYSTEM	9 custom_section: fromhost-ip=192.168.45.219
MSG

# Test case 4: Large number (999999999)
cat <<'MSG' > ${RSYSLOG_DYNNAME}.input4
<14>Mar 22 08:47:26 testhost MSWinEventLog	1	Microsoft-Windows-Sysmon/Operational	20980	Mon Mar 22 08:47:26 2025	13	Windows	SYSTEM	User	SetValue	testhost	Registry value set (rule: RegistryEvent)		Registry value set:  RuleName: Default RegistryEvent  EventType: SetValue  UtcTime: 2025-03-22 08:47:26.284  ProcessGuid: {fd4d0da6-d589-6916-eb03-000000000003}  ProcessId: 4  Image: System  TargetObject: HKLM\System\CurrentControlSet\Services\TestService\ImagePath  Details: "C:\Program Files\TestAgent\TestService.exe"  User: NT AUTHORITY\SYSTEM	999999999 custom_section: fromhost-ip=192.168.45.220
MSG

# Test case 5: Multiple spaces (should still match)
cat <<'MSG' > ${RSYSLOG_DYNNAME}.input5
<14>Mar 22 08:47:27 testhost MSWinEventLog	1	Microsoft-Windows-Sysmon/Operational	20981	Mon Mar 22 08:47:27 2025	13	Windows	SYSTEM	User	SetValue	testhost	Registry value set (rule: RegistryEvent)		Registry value set:  RuleName: Default RegistryEvent  EventType: SetValue  UtcTime: 2025-03-22 08:47:27.284  ProcessGuid: {fd4d0da6-d589-6916-eb03-000000000004}  ProcessId: 4  Image: System  TargetObject: HKLM\System\CurrentControlSet\Services\TestService\ImagePath  Details: "C:\Program Files\TestAgent\TestService.exe"  User: NT AUTHORITY\SYSTEM	42   custom_section: fromhost-ip=192.168.45.221
MSG

# Test case 6: Zero-padded number (000123)
cat <<'MSG' > ${RSYSLOG_DYNNAME}.input6
<14>Mar 22 08:47:28 testhost MSWinEventLog	1	Microsoft-Windows-Sysmon/Operational	20982	Mon Mar 22 08:47:28 2025	13	Windows	SYSTEM	User	SetValue	testhost	Registry value set (rule: RegistryEvent)		Registry value set:  RuleName: Default RegistryEvent  EventType: SetValue  UtcTime: 2025-03-22 08:47:28.284  ProcessGuid: {fd4d0da6-d589-6916-eb03-000000000005}  ProcessId: 4  Image: System  TargetObject: HKLM\System\CurrentControlSet\Services\TestService\ImagePath  Details: "C:\Program Files\TestAgent\TestService.exe"  User: NT AUTHORITY\SYSTEM	000123 custom_section: fromhost-ip=192.168.45.222
MSG

injectmsg_file ${RSYSLOG_DYNNAME}.input1
injectmsg_file ${RSYSLOG_DYNNAME}.input2
injectmsg_file ${RSYSLOG_DYNNAME}.input3
injectmsg_file ${RSYSLOG_DYNNAME}.input4
injectmsg_file ${RSYSLOG_DYNNAME}.input5
injectmsg_file ${RSYSLOG_DYNNAME}.input6

shutdown_when_empty
wait_shutdown

# Test that Event ID 13 (Registry value set) is parsed correctly for all test cases
# The ignoreTrailingPattern.regex should remove the custom sections with dynamic
# numeric prefixes from the message before parsing, so they should NOT appear in any
# parsed fields. However, the truncated content is stored in the !extradata_section property.
# This test verifies:
# 1. Parsing works correctly (EventID=13, Channel, EventType=SetValue, TargetObject, User)
# 2. The custom sections with various numeric prefixes are removed from parsing
# 3. The truncated content (including the numeric prefix) is stored in !extradata_section property

# Test case 1: Standard number (3385599)
content_check '13,Microsoft-Windows-Sysmon/Operational,SetValue,HKLM\System\CurrentControlSet\Services\TestService\ImagePath,NT AUTHORITY\SYSTEM,3385599 custom_section: fromhost-ip=192.168.45.217' $RSYSLOG_OUT_LOG

# Test case 2: Different number (12345)
content_check '13,Microsoft-Windows-Sysmon/Operational,SetValue,HKLM\System\CurrentControlSet\Services\TestService\ImagePath,NT AUTHORITY\SYSTEM,12345 custom_section: fromhost-ip=192.168.45.218' $RSYSLOG_OUT_LOG

# Test case 3: Single digit (9)
content_check '13,Microsoft-Windows-Sysmon/Operational,SetValue,HKLM\System\CurrentControlSet\Services\TestService\ImagePath,NT AUTHORITY\SYSTEM,9 custom_section: fromhost-ip=192.168.45.219' $RSYSLOG_OUT_LOG

# Test case 4: Large number (999999999)
content_check '13,Microsoft-Windows-Sysmon/Operational,SetValue,HKLM\System\CurrentControlSet\Services\TestService\ImagePath,NT AUTHORITY\SYSTEM,999999999 custom_section: fromhost-ip=192.168.45.220' $RSYSLOG_OUT_LOG

# Test case 5: Multiple spaces (42   custom_section:)
content_check '13,Microsoft-Windows-Sysmon/Operational,SetValue,HKLM\System\CurrentControlSet\Services\TestService\ImagePath,NT AUTHORITY\SYSTEM,42   custom_section: fromhost-ip=192.168.45.221' $RSYSLOG_OUT_LOG

# Test case 6: Zero-padded number (000123)
content_check '13,Microsoft-Windows-Sysmon/Operational,SetValue,HKLM\System\CurrentControlSet\Services\TestService\ImagePath,NT AUTHORITY\SYSTEM,000123 custom_section: fromhost-ip=192.168.45.222' $RSYSLOG_OUT_LOG

exit_test
