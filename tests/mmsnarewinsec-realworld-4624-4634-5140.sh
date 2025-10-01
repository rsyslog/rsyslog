#!/bin/bash
# Test real-world Windows events (4624, 4634, 5140) with runtime field override for "Source Address"
unset RSYSLOG_DYNNAME
. ${srcdir:=.}/diag.sh init

DEF_FILE="${RSYSLOG_DYNNAME}.defs.json"
cat >"$DEF_FILE" <<'JSON'
{
  "fields": [
    {
      "pattern": "Source Address",
      "canonical": "SourceNetworkAddress",
      "section": "Network",
      "value_type": "ip_address",
      "priority": 80
    }
  ]
}
JSON

generate_conf
add_conf '
module(load="../plugins/mmsnarewinsec/.libs/mmsnarewinsec" definition.file="'${PWD}/${DEF_FILE}'")

template(name="outjson" type="list" option.jsonf="on") {
  property(outname="EventID" name="$!win!Event!EventID" format="jsonf")
  property(outname="ClientIP" name="$!win!Network!SourceNetworkAddress" format="jsonf")
  property(outname="ClientPort" name="$!win!Network!SourcePort" format="jsonf")
  property(outname="LogonTypeLogonInfo" name="$!win!LogonInformation!LogonType" format="jsonf")
  property(outname="LogonTypeNameLogonInfo" name="$!win!LogonInformation!LogonTypeName" format="jsonf")
  property(outname="LogonTypeEventData" name="$!win!EventData!LogonType" format="jsonf")
  property(outname="LogonTypeNameEventData" name="$!win!EventData!LogonTypeName" format="jsonf")
}

action(type="mmsnarewinsec")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outjson")
'

startup

cat > ${RSYSLOG_DYNNAME}.input <<'DATA'
<13>1 2025-09-18T13:03:42.970090Z foobazhost - - - - MSWinEventLog	1	Security	284138676	Thu Sep 18 13:03:42 2025	4624	Windows	N/A	N/A	Success Audit	foobazhost	Logon		An account was successfully logged on.    Subject:   Security ID:  S-1-0-0   Account Name:  -   Account Domain:  -   Logon ID:  0x0    Logon Information:   Logon Type:  3   Restricted Admin Mode: -   Virtual Account:  %%1843   Elevated Token:  %%1842    Impersonation Level:  %%1840    New Logon:   Security ID:  S-X-X-XX-XXXXXXXXXX-XXXXXXXXXX-XXXXXXXXXX-XXXXXX   Account Name:  FOOHOST$   Account Domain:  DOMAIN   Logon ID:  0xxxxxxxxxx   Linked Logon ID:  0x0   Network Account Name: -   Network Account Domain: -   Logon GUID:  {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}    Process Information:   Process ID:  0x0   Process Name:  -    Network Information:   Workstation Name: -   Source Network Address: 10.10.10.10   Source Port:  62029    Detailed Authentication Information:   Logon Process:  Kerberos   Authentication Package: Kerberos   Transited Services: -   Package Name (NTLM only): -   Key Length:  0   	-15049365
<13>1 2025-09-18T13:02:46.038710Z foobarhost - - - - MSWinEventLog	1	Security	29352898	Thu Sep 18 13:02:45 2025	4634	Windows	N/A	N/A	Success Audit	foobarhost	Logoff		An account was logged off.    Subject:   Security ID:  S-X-X-XX-XXXXXXXXXX-XXXXXXXXXX-XXXXXXXXXX-XXXXX   Account Name:  BLUB$   Account Domain:  DOMAIN   Logon ID:  0xxxxxxxxx    Logon Type:   3   	1248938155
<13>1 2025-09-18T13:16:50.825272Z blubhost - - - - MSWinEventLog	1	Security	111948961	Thu Sep 18 13:16:49 2025	5140	Windows	N/A	N/A	Success Audit	blubhost	File Share		A network share object was accessed.     Subject:   Security ID:  S-X-X-XX-XXXXXXXXXX-XXXXXXXXXXX-XXXXXXXXXX-XXXX   Account Name:  USER1   Account Domain:  DOMAIN   Logon ID:  0xxxxxxxxxx    Network Information:    Object Type:  File   Source Address:  10.10.10.10   Source Port:  57814     Share Information:   Share Name:  \\*\ABC$   Share Path:      Access Request Information:   Access Mask:  0x1   Accesses:  %%4416   	2047653216
DATA

injectmsg_file ${RSYSLOG_DYNNAME}.input

shutdown_when_empty
wait_shutdown

# 5140 assertions
content_check '"eventid":"5140"' $RSYSLOG_OUT_LOG
content_check '"clientip":"10.10.10.10"' $RSYSLOG_OUT_LOG
content_check '"clientport":"57814"' $RSYSLOG_OUT_LOG

# 4624 assertions
content_check '"eventid":"4624"' $RSYSLOG_OUT_LOG
content_check '"clientip":"10.10.10.10"' $RSYSLOG_OUT_LOG
content_check '"clientport":"62029"' $RSYSLOG_OUT_LOG
content_check '"logontypelogoninfo":"3"' $RSYSLOG_OUT_LOG
content_check '"logontypenamelogoninfo":"Network"' $RSYSLOG_OUT_LOG

# 4634 presence
content_check '"eventid":"4634"' $RSYSLOG_OUT_LOG

rm -f "$DEF_FILE"

exit_test

