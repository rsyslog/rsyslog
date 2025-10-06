#!/bin/bash
# Validate mmsnareparse parsing against representative NXLog Security samples.
unset RSYSLOG_DYNNAME
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmsnareparse/.libs/mmsnareparse")

template(name="outfmt" type="list") {
    property(name="$!win!Event!EventID")
    constant(value=",")
    property(name="$!win!LogonInformation!LogonType")
    constant(value=",")
    property(name="$!win!LogonInformation!LogonTypeName")
    constant(value=",")
    property(name="$!win!LAPS!PolicyVersion")
    constant(value=",")
    property(name="$!win!LAPS!CredentialRotation")
    constant(value=",")
    property(name="$!win!TLSInspection!Reason")
    constant(value=",")
    property(name="$!win!WDAC!PolicyVersion")
    constant(value=",")
    property(name="$!win!WDAC!PID")
    constant(value=",")
    property(name="$!win!WUFB!PolicyID")
    constant(value=",")
    property(name="$!win!Logon!RemoteCredentialGuard")
    constant(value=",")
    property(name="$!win!Network!SourcePort")
    constant(value="\n")
}

action(type="mmsnareparse")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

startup
cat <<'MSG' > ${RSYSLOG_DYNNAME}.input
<13>1 2025-02-18T06:42:17.554128Z DC25-PREVIEW - - - - MSWinEventLog	1	Security	802301	Tue Feb 18 06:42:17 2025	4624	Microsoft-Windows-Security-Auditing	N/A	N/A	Success Audit	DC25-PREVIEW	Logon		An account was successfully logged on.    Subject:   Security ID:  S-1-5-18   Account Name:  SYSTEM   Account Domain:  NT AUTHORITY   Logon ID:  0x3E7    Logon Information:   Logon Type:  2   Restricted Admin Mode: -   Virtual Account:  %%1843   Elevated Token:  %%1843    New Logon:   Security ID:  S-1-5-21-88997766-1122334455-6677889900-500   Account Name:  ADMIN-LAPS$   Account Domain:  FABRIKAM   Logon ID:  0x52F1A   Linked Logon ID:  0x0   Network Account Name: -   Network Account Domain: -   Logon GUID:  {5a8f0679-9b23-4cb7-a8c7-3d650c9b52ec}    Process Information:   Process ID:  0x66c   Process Name:  C:\Windows\System32\winlogon.exe    Network Information:   Workstation Name:  CORE25-01   Source Network Address: 192.168.50.12   Source Port:  59122    Detailed Authentication Information:   Logon Process:  User32   Authentication Package:  Negotiate   Transited Services: -   Package Name (NTLM only): -   Key Length:  0    Remote Credential Guard:  Enabled    LAPS Context:  PolicyVersion=2; CredentialRotation=True   	-802301
<13>1 2025-02-18T07:01:55.771903Z EDGE25-01 - - - - MSWinEventLog	1	Security	301221	Tue Feb 18 07:01:55 2025	5157	Microsoft-Windows-Security-Auditing	N/A	N/A	Failure Audit	EDGE25-01	Filtering Platform Packet Drop		The Windows Filtering Platform has blocked a connection.    Application Information:   Process ID:  948   Application Name:  C:\Program Files\Contoso\edgegateway.exe    Network Information:   Direction:  Outbound   Source Address:  10.15.5.20   Source Port:  57912   Destination Address:  104.45.23.110   Destination Port:  443   Protocol:  6    Filter Information:   Filter Run-Time ID:  89041   Layer Name:  %%14596   Layer Run-Time ID:  44    TLS Inspection:   Reason:  Unapproved Root Authority   Policy:  ContosoOutboundTLS   	-301221
<13>1 2025-02-18T07:05:44.888234Z APP25-API - - - - MSWinEventLog	1	Security	402991	Tue Feb 18 07:05:44 2025	6281	Microsoft-Windows-CodeIntegrity	N/A	N/A	Error	APP25-API	Application Control		Code Integrity determined that a process (\Device\HarddiskVolume4\Program Files\LegacyERP\erp.exe) attempted to load \Device\HarddiskVolume4\Temp\unsigned.dll that did not meet the Enterprise signing level requirements.    Policy Name:  FABRIKAM-WDAC-BaseV3   Policy Version:  3.2.0   Enforcement Mode:  Audit+Enforce   User:  FABRIKAM\svc_batch   PID:  4128   	-402991
<13>1 2025-02-18T06:59:13.332719Z DC25-PREVIEW - - - - MSWinEventLog	1	Security	802340	Tue Feb 18 06:59:13 2025	1243	Microsoft-Windows-WindowsUpdateClient	N/A	N/A	Information	DC25-PREVIEW	WUFB Deployment		Windows Update for Business deployment policy enforced.    Policy ID:  2f9c4414-3f71-4f2b-9a7e-cc98a6d96970   Ring:  SecureBaseline   From Service:  Windows Update for Business deployment service   Enforcement Result:  Success   	-802340
MSG
injectmsg_file ${RSYSLOG_DYNNAME}.input

shutdown_when_empty
wait_shutdown

content_check '4624,2,Interactive,2,true,,,,,true,59122' $RSYSLOG_OUT_LOG
content_check '5157,,,,,Unapproved Root Authority,,,,,57912' $RSYSLOG_OUT_LOG
content_check '6281,,,,,,3.2.0,4128,,,' $RSYSLOG_OUT_LOG
content_check '1243,,,,,,,,2f9c4414-3f71-4f2b-9a7e-cc98a6d96970,,' $RSYSLOG_OUT_LOG

exit_test
