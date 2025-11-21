#!/bin/bash
# Validate mmsnareparse parsing with trailing extra-data section truncation.
# This test verifies both the with-tabs code path and documents the bug fix for the no-tabs path.
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
       ignoreTrailingPattern="enrichment_section:")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

startup
cat <<'MSG' > ${RSYSLOG_DYNNAME}.input
<14>Mar 22 08:47:23 testhost MSWinEventLog	1	Microsoft-Windows-Sysmon/Operational	20977	Mon Mar 22 08:47:23 2025	13	Windows	SYSTEM	User	SetValue	testhost	Registry value set (rule: RegistryEvent)		Registry value set:  RuleName: Default RegistryEvent  EventType: SetValue  UtcTime: 2025-03-22 08:47:23.284  ProcessGuid: {fd4d0da6-d589-6916-eb03-000000000000}  ProcessId: 4  Image: System  TargetObject: HKLM\System\CurrentControlSet\Services\TestService\ImagePath  Details: "C:\Program Files\TestAgent\TestService.exe"  User: NT AUTHORITY\SYSTEM	3385599 enrichment_section: fromhost-ip=192.168.45.217
MSG
injectmsg_file ${RSYSLOG_DYNNAME}.input

shutdown_when_empty
wait_shutdown

# Test that Event ID 13 (Registry value set) is parsed correctly
# The ignoreTrailingPattern should remove "enrichment_section: fromhost-ip=192.168.45.217" 
# from the message before parsing, so it should NOT appear in any parsed fields.
# However, the truncated content is stored in the !extradata_section property.
# This test verifies:
# 1. Parsing works correctly (EventID=13, Channel, EventType=SetValue, TargetObject, User)
# 2. The enrichment section is removed from parsing (doesn't affect parsed fields)
# 3. The truncated content is stored in !extradata_section property (tests with-tabs code path)
#
# NOTE: A critical bug in the no-tabs code path (lines 5111-5121 in mmsnareparse.c) was fixed
# where strdup was called AFTER truncation, resulting in an empty string. The fix reverses
# the order: copy first, then truncate. This is now consistent with the with-tabs path.
content_check '13,Microsoft-Windows-Sysmon/Operational,SetValue,HKLM\System\CurrentControlSet\Services\TestService\ImagePath,NT AUTHORITY\SYSTEM,3385599 enrichment_section: fromhost-ip=192.168.45.217' $RSYSLOG_OUT_LOG

exit_test
