#!/bin/bash
# Validate that mmsnareparse regex trailing extra-data detection does not let
# the search-window boundary act as the end of the trailing token. The oracle is
# that an end-anchored pattern must not match only the bounded prefix of a longer
# non-matching token, so the final token remains parsed data and no
# extradata_section is emitted for it.
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
    property(name="$!win!EventData!User")
    constant(value=",")
    property(name="$!extradata_section")
    constant(value="\n")
}

action(type="mmsnareparse"
       definition.file="../plugins/mmsnareparse/sysmon_definitions.json"
       ignoreTrailingPattern.regex="^[0-9]+$"
       ignoreTrailingPattern.searchWindow="3")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

startup

cat <<'MSG' > ${RSYSLOG_DYNNAME}.input
<14>Mar 22 08:47:23 testhost MSWinEventLog	1	Microsoft-Windows-Sysmon/Operational	20977	Mon Mar 22 08:47:23 2025	13	Windows	SYSTEM	User	SetValue	testhost	Registry value set (rule: RegistryEvent)		Registry value set:  RuleName: Default RegistryEvent  EventType: SetValue  UtcTime: 2025-03-22 08:47:23.284  ProcessGuid: {fd4d0da6-d589-6916-eb03-000000000000}  ProcessId: 4  Image: System  TargetObject: HKLM\System\CurrentControlSet\Services\TestService\ImagePath  Details: "C:\Program Files\TestAgent\TestService.exe"  User: NT AUTHORITY\SYSTEM	123abc
MSG

injectmsg_file ${RSYSLOG_DYNNAME}.input

shutdown_when_empty
wait_shutdown

content_check '13,Microsoft-Windows-Sysmon/Operational,NT AUTHORITY\SYSTEM 123abc,' "$RSYSLOG_OUT_LOG"

exit_test
