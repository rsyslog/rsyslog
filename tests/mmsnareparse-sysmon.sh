#!/bin/bash
# Validate mmsnareparse parsing against Microsoft Sysmon events using definition file.
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
    property(name="$!win!Event!Category")
    constant(value=",")
    property(name="$!win!Event!Subtype")
    constant(value=",")
    property(name="$!win!EventData!ProcessId")
    constant(value=",")
    property(name="$!win!EventData!Image")
    constant(value=",")
    property(name="$!win!EventData!CommandLine")
    constant(value=",")
    property(name="$!win!EventData!User")
    constant(value=",")
    property(name="$!win!Network!SourceIp")
    constant(value=",")
    property(name="$!win!Network!SourcePort")
    constant(value=",")
    property(name="$!win!Network!DestinationIp")
    constant(value=",")
    property(name="$!win!Network!DestinationPort")
    constant(value=",")
    property(name="$!win!Network!Protocol")
    constant(value="\n")
}

action(type="mmsnareparse"
       definition.file="../plugins/mmsnareparse/sysmon_definitions.json")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

startup
cat <<'MSG' > ${RSYSLOG_DYNNAME}.input
<14>Nov 25 05:54:32 DC-01 MSWinEventLog	1	Microsoft-Windows-Sysmon/Operational	10448	Tue Nov 25 05:54:32 2025	1	Windows	SYSTEM	User	Information	DC-01	CORP\NETWORK SERVICE	Process creation	Sysmon  Event ID 1 - Process creation: UtcTime: 2024-04-28 22:08:22.025 ProcessGuid: {b34fbf9a-ce67-6a14-1111-1121f0a06f11} ProcessId: 6228 Image: C:\Windows\System32\wbem\WmiPrvSE.exe FileVersion: 10.0.22621.1 (WinBuild.160101.0800) Description: WMI Provider Host Product: Microsoft® Windows® Operating System Company: Microsoft Corporation OriginalFileName: Wmiprvse.exe CommandLine: C:\Windows\system32\wbem\wmiprvse.exe -secured -Embedding CurrentDirectory: C:\Windows\system32\ User: CORP\NETWORK SERVICE LogonGuid: {d56hdh1c-eg89-8c36-3333-3343h2c28h33} LogonId: 0x7EB05 TerminalSessionId: 1 IntegrityLevel: System Hashes: SHA1=A3F7B2C8D9E1F4A5B6C7D8E9F0A1B2C3D4E5F6A7,MD5=8E4F2A1B3C5D6E7F8A9B0C1D2E3F4A5,SHA256=7B9C2D4E5F6A7B8C9D0E1F2A3B4C5D6E7F8A9B0C1D2E3F4A5B6C7D8E9F0A1B2C3D4,IMPHASH=6A5B4C3D2E1F0A9B8C7D6E5F4A3B2C1D0E9F8A7B6C5 ParentProcessGuid: {c45gcg0b-df78-7b25-2222-2232g1b17g22} ParentProcessId: 580 ParentImage: C:\Windows\System32\svchost.exe ParentCommandLine: C:\Windows\system32\svchost.exe -k DcomLaunch -p ParentUser: NT INTERNAL\SYSTEM	10448
<14>Mar 10 22:36:54 SQL-01 MSWinEventLog	1	Microsoft-Windows-Sysmon/Operational	30692	Mon Mar 10 22:36:54 2025	3	Windows	SYSTEM	User	Information	SQL-01	LAB\Administrator	Network connection detected	Sysmon  Event ID 3 - Network connection detected: RuleName: RDP UtcTime: 2017-04-28 22:12:22.557 ProcessGuid: {c45gcg0b-df78-7b25-2222-2232g1b17g22} ProcessId: 13220 Image: C:\Program Files (x86)\Google\Chrome\Application\chrome.exe Description: Sysmon  Event ID 3 User: LAB\Administrator SourceIp: 10.0.0.20 SourcePort: 3328 DestinationIp: 192.168.1.20 DestinationPort: 3389 Protocol: tcp Initiated: true SourceIsIpv6: 192.168.1.20 SourceHostname: DC-03 DestinationIsIpv6: 10.0.0.20 DestinationPortName: ms-wbt-server	30692
<14>Jun 27 04:55:00 SERVER-01 MSWinEventLog	1	Microsoft-Windows-Sysmon/Operational	50518	Fri Jun 27 04:55:00 2025	5	Windows	SYSTEM	User	Information	SERVER-01		Process terminated	Sysmon  Event ID 5 - Process terminated: UtcTime: 2017-04-28 22:13:20.895 ProcessGuid: {02de4fd3-bd56-5903-0000-0010e9d95e00} ProcessId: 12684 Image: C:\Program Files (x86)\Google\Chrome\Application\chrome.exe Description: Sysmon  Event ID 5	50518
MSG
injectmsg_file ${RSYSLOG_DYNNAME}.input

shutdown_when_empty
wait_shutdown

# Test Event ID 1 (Process Creation)
content_check '1,Microsoft-Windows-Sysmon/Operational,Process,Creation,6228,C:\Windows\System32\wbem\WmiPrvSE.exe,C:\Windows\system32\wbem\wmiprvse.exe -secured -Embedding,CORP\NETWORK SERVICE,,,,,' $RSYSLOG_OUT_LOG

# Test Event ID 3 (Network Connection)
content_check '3,Microsoft-Windows-Sysmon/Operational,Network,Connection,13220,C:\Program Files (x86)\Google\Chrome\Application\chrome.exe,,LAB\Administrator,10.0.0.20,,192.168.1.20,,tcp' $RSYSLOG_OUT_LOG

# Test Event ID 5 (Process Termination)
content_check '5,Microsoft-Windows-Sysmon/Operational,Process,Termination,12684,C:\Program Files (x86)\Google\Chrome\Application\chrome.exe,,,,,,' $RSYSLOG_OUT_LOG

exit_test
