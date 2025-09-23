#!/bin/bash
# added 2025-09-?? by AI assistant, released under ASL 2.0
# Basic parsing test for the mmleefparse module using Palo Alto Networks samples.
# The regression covers allow, drop and reset-both events provided with the task.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=3
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
module(load="../plugins/mmleefparse/.libs/mmleefparse")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.tcpflood_port")


#template(name="outfmt" type="subtree" subtree="$!leef" option.forceEndOfLine="on")

template(name="outfmt" type="string" string="%$!leef!header!vendor%|%$!leef!header!productVersion%|%$!leef!fields!src%|%$!leef!fields!dst%|%$!leef!fields!action%|%$!leef!fields!cat%|%$!leef!fields!proto%|%$!leef!fields!SessionID%\n")

if $syslogtag == "LEEF:" then {
  action(type="mmleefparse" container="!leef" delimiter="|")
  if $parsesuccess == "OK" then {
          action(name="write-leef" type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
  } else {
          action(name="write-non-leef" type="omfile" file=`echo $RSYSLOG2_OUT_LOG`)
  }
}
'

startup
tcpflood -m1 -M "\"<14>Sep 17 13:45:35 firewall.domain.local LEEF:1.0|Palo Alto Networks|PAN-OS Syslog Integration|11.1.6-h14|allow|cat=TRAFFIC|ReceiveTime=2025/09/17 13:45:34|SerialNumber=010108010025|Type=TRAFFIC|Subtype=start|devTime=Sep 17 2025 11:45:34 GMT|src=172.19.50.39|dst=172.19.5.50|srcPostNAT=0.0.0.157|dstPostNAT=0.0.0.157|RuleName=interzone-default|usrName=|SourceUser=|DestinationUser=|Application=ssl|VirtualSystem=vsys3|SourceZone=x-def|DestinationZone=z-def|IngressInterface=iu1.6751|EgressInterface=iu1.6723|LogForwardingProfile=default|SessionID=74879677|RepeatCount=1|srcPort=62126|dstPort=443|srcPostNATPort=0|dstPostNATPort=0|Flags=0x0|proto=tcp|action=allow|totalBytes=460|dstBytes=70|srcBytes=390|totalPackets=4|StartTime=2025/09/17 13:45:33|ElapsedTime=0|URLCategory=any|sequence=7548503197945820434|ActionFlags=0x8000000000000000|SourceLocation=172.0.0.20-172.255.255.1|DestinationLocation=172.0.0.20-172.255.255.1|dstPackets=1|srcPackets=3|SessionEndReason=n/a|DeviceGroupHierarchyL1=177|DeviceGroupHierarchyL2=235|DeviceGroupHierarchyL3=236|DeviceGroupHierarchyL4=239|vSrcName=firewall.domain.local|DeviceName=firewall.domain.local|ActionSource=from-policy|SrcUUID=|DstUUID=|TunnelID=0|MonitorTag=|ParentSessionID=0|ParentStartTime=|TunnelType=N/A\""
tcpflood -m1 -M "\"<14>Sep 17 13:48:38 firewall.domain.local LEEF:1.0|Palo Alto Networks|PAN-OS Syslog Integration|11.1.6-h14|deny|cat=TRAFFIC|ReceiveTime=2025/09/17 13:48:37|SerialNumber=010108010025|Type=TRAFFIC|Subtype=drop|devTime=Sep 17 2025 11:48:37 GMT|src=172.39.129.169|dst=172.224.141.39|srcPostNAT=0.0.0.157|dstPostNAT=0.0.0.157|RuleName=interzone-default|usrName=|SourceUser=|DestinationUser=|Application=not-applicable|VirtualSystem=vsys7|SourceZone=wan|DestinationZone=partner-gw|IngressInterface=zz8.2012|EgressInterface=|LogForwardingProfile=default|SessionID=0|RepeatCount=1|srcPort=63219|dstPort=3367|srcPostNATPort=0|dstPostNATPort=0|Flags=0x0|proto=tcp|action=deny|totalBytes=0|dstBytes=0|srcBytes=0|totalPackets=1|StartTime=2025/09/17 13:48:38|ElapsedTime=0|URLCategory=any|sequence=7548503197948374938|ActionFlags=0x8000000000000000|SourceLocation=172.0.0.20-172.255.255.1|DestinationLocation=172.0.0.20-172.255.255.1|dstPackets=0|srcPackets=1|SessionEndReason=policy-deny|DeviceGroupHierarchyL1=177|DeviceGroupHierarchyL2=235|DeviceGroupHierarchyL3=48936|DeviceGroupHierarchyL4=36126|vSrcName=firewall.domain.local|DeviceName=firewall.domain.local|ActionSource=from-policy|SrcUUID=|DstUUID=|TunnelID=0|MonitorTag=|ParentSessionID=0|ParentStartTime=|TunnelType=N/A\""
tcpflood -m1 -M "\"<14>Sep 17 13:47:11 firewall.domain.local LEEF:1.0|Palo Alto Networks|PAN-OS Syslog Integration|11.1.6-h14|reset-both|cat=TRAFFIC|ReceiveTime=2025/09/17 13:47:10|SerialNumber=010108000243|Type=TRAFFIC|Subtype=deny|devTime=Sep 17 2025 11:47:10 GMT|src=172.35.163.110|dst=172.42.1.234|srcPostNAT=0.0.0.157|dstPostNAT=0.0.0.157|RuleName=interzone-default|usrName=|SourceUser=|DestinationUser=|Application=ssl|VirtualSystem=vsys7|SourceZone=client-vpn|DestinationZone=gdcbb|IngressInterface=zz8.1083|EgressInterface=zz8.1001|LogForwardingProfile=default|SessionID=336519782|RepeatCount=1|srcPort=61567|dstPort=9997|srcPostNATPort=0|dstPostNATPort=0|Flags=0x100400|proto=tcp|action=reset-both|totalBytes=6689|dstBytes=5964|srcBytes=725|totalPackets=14|StartTime=2025/09/17 13:47:10|ElapsedTime=0|URLCategory=any|sequence=7548134123908109690|ActionFlags=0x8000000000000000|SourceLocation=172.0.0.20-172.255.255.1|DestinationLocation=172.0.0.20-172.255.255.1|dstPackets=7|srcPackets=7|SessionEndReason=policy-deny|DeviceGroupHierarchyL1=177|DeviceGroupHierarchyL2=235|DeviceGroupHierarchyL3=48936|DeviceGroupHierarchyL4=36126|vSrcName=firewall.domain.local|DeviceName=firewall.domain.local|ActionSource=from-application|SrcUUID=|DstUUID=|TunnelID=0|MonitorTag=|ParentSessionID=0|ParentStartTime=|TunnelType=N/A\""
shutdown_when_empty
wait_shutdown

EXPECTED=$'Palo Alto Networks|11.1.6-h14|172.19.50.39|172.19.5.50|allow|TRAFFIC|tcp|74879677\nPalo Alto Networks|11.1.6-h14|172.39.129.169|172.224.141.39|deny|TRAFFIC|tcp|0\nPalo Alto Networks|11.1.6-h14|172.35.163.110|172.42.1.234|reset-both|TRAFFIC|tcp|336519782'
cmp_exact

if [ -s "$RSYSLOG2_OUT_LOG" ]; then
        echo "unexpected data in secondary output log:"
        cat "$RSYSLOG2_OUT_LOG"
        error_exit 1
fi

exit_test
