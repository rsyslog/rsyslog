#!/bin/bash
# added 2025-10-06 by rgerhards
# check hypothetical data pipeline w/ qradar and unflatten transformation
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"	"Does not work with Solaris Python versions - not worth working around"
generate_conf
add_conf '
template(name="outfmt" type="subtree" subtree="$!qradar_structured")

module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/mmjsontransform/.libs/mmjsontransform")

# useRawMsg="on" is best practice if syslog header is not known to be present or not.
action(type="mmjsonparse" container="$!qradar" mode="find-json" useRawMsg="on")
if $parsesuccess == "OK" then {
  action(type="mmjsontransform" input="$!qradar" output="$!qradar_structured" mode="unflatten")
  # upstream "shipper" here is just a file
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
injectmsg_file $srcdir/testsuites/qradar_json-with-dots
shutdown_when_empty
wait_shutdown

# below is a very long line for checking generated json. This is not nice,
# but helpful to get us going. This should be replaced by a json compare
# tool callable via diag.sh.
export EXPECTED='{ "name": "DefaultProfile", "version": "1.0", "isoTimeFormat": "yyyy-MM-ddTHH:mm:ss.SSSZ", "type": "Event", "category": "4688", "protocolID": "255", "sev": "2", "src": { "ip": "10.5.14.81", "Port": "0" }, "dst": { "ip": "10.5.14.81", "Port": "0" }, "relevance": "5", "credibility": "5", "startTimeEpoch": "1759325971476", "startTimeISO": "2025-10-01T13:39:31.476Z", "storageTimeEpoch": "1759325971476", "storageTimeISO": "2025-10-01T13:39:31.476Z", "deploymentID": "1111aaa3-08a1-11eb-80f7-ecebb11d9a14", "devTimeEpoch": "1759325920000", "devTimeISO": "2025-10-01T13:38:40.000Z", "srcPreNATPort": "0", "dstPreNATPort": "0", "srcPostNATPort": "0", "dstPostNATPort": "0", "hasIdentity": "false", "payload": "<14>Oct  1 13:38:40 abcddul23105 MSWinEventLog\t1\tSecurity\t27884\tWed Oct 01 13:38:40 2025\t4688\tWindows\tN\/A\tN\/A\tSuccess Audit\tabcddul23105\tProcess Creation\t\tA new process has been created.    Creator Subject:   Security ID:  NT AUTHORITY\\SYSTEM   Account Name:  abcdDUL23105$ Account Domain:  DOMAIN   Logon ID:  0x3E7    Target Subject:   Security ID:  DOMAIN\\FOOBAR   Account Name:  FOOBAR   Account Domain:  DOMAIN   Logon ID:  0x19C34    Process Information:   New Process ID:  0x27a8   New Process Name: C:\\Windows\\System32\\backgroundTaskHost.exe   Token Elevation Type: TokenElevationTypeDefault (1)   Mandatory Label:  Mandatory Label\\Low Mandatory Level   Creator Process ID: 0x4b0   Creator Process Name: C:\\Windows\\System32\\svchost.exe   Process Command Line: \"C:\\WINDOWS\\system32\\BackgroundTaskHost.exe\" -ServerName:BackgroundTaskHost.WebAccountProvider   \t7574751\tenrichment_section: fromhost-ip=10.5.14.81\n", "eventCnt": "1", "hasOffense": "false", "domainID": "0", "eventName": "Success Audit: A new process has been created", "lowLevelCategory": "Process Creation Success", "highLevelCategory": "System", "eventDescription": "Success Audit: A new process has been created.", "srcAssetName": "SERVER", "dstAssetName": "SERVER", "logSource": "abcddul23105", "srcNetName": "Net-10-172-192.Net_10_0_0_0", "dstNetName": "Net-10-172-192.Net_10_0_0_0", "logSourceType": "Microsoft Windows Security Event Log", "logSourceGroup": "THE_GROUP", "logSourceIdentifier": "abcddul23105", "Target User Name": "FOOBAR", "EventID": "4688", "Source Process": "backgroundTaskHost.exe", "Parent Process Name": "svchost.exe", "Process CommandLine": "\"C:\\WINDOWS\\system32\\BackgroundTaskHost.exe\" -ServerName:BackgroundTaskHost.WebAccountProvider", "Parent Process Path": "C:\\Windows\\System32\\svchost.exe" }'
cmp_exact
exit_test
