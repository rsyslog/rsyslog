#!/bin/bash
# Valgrind-enabled comprehensive test for mmsnareparse field extraction
# Mirrors mmsnareparse-comprehensive.sh but runs rsyslogd under valgrind to
# surface memory issues such as the reported heap-buffer-overflow.

unset RSYSLOG_DYNNAME
. ${srcdir:=.}/diag.sh init

# hint to diag harness that valgrind output is required for this test
export USE_VALGRIND="YES"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmsnareparse/.libs/mmsnareparse")

# Template to extract comprehensive structured JSON output
template(name="jsonfmt" type="list" option.jsonf="on") {
    # Event fields
    property(outname="eventid" name="$!win!Event!EventID" format="jsonf")
    property(outname="channel" name="$!win!Event!Channel" format="jsonf")
    property(outname="eventtype" name="$!win!Event!EventType" format="jsonf")
    property(outname="categorytext" name="$!win!Event!CategoryText" format="jsonf")
    property(outname="computer" name="$!win!Event!Computer" format="jsonf")
    property(outname="provider" name="$!win!Event!Provider" format="jsonf")

    # Subject fields
    property(outname="subjectsecurityid" name="$!win!Subject!SecurityID" format="jsonf")
    property(outname="subjectaccountname" name="$!win!Subject!AccountName" format="jsonf")
    property(outname="subjectaccountdomain" name="$!win!Subject!AccountDomain" format="jsonf")
    property(outname="subjectlogonid" name="$!win!Subject!LogonID" format="jsonf")

    # LogonInformation fields
    property(outname="logontype" name="$!win!LogonInformation!LogonType" format="jsonf")
    property(outname="logontypename" name="$!win!LogonInformation!LogonTypeName" format="jsonf")
    property(outname="restrictedadminmode" name="$!win!LogonInformation!RestrictedAdminMode" format="jsonf")
    property(outname="virtualaccount" name="$!win!LogonInformation!VirtualAccount" format="jsonf")
    property(outname="elevatedtoken" name="$!win!LogonInformation!ElevatedToken" format="jsonf")
    property(outname="impersonationlevel" name="$!win!LogonInformation!ImpersonationLevel" format="jsonf")

    # NewLogon fields
    property(outname="newlogonsecurityid" name="$!win!NewLogon!SecurityID" format="jsonf")
    property(outname="newlogonaccountname" name="$!win!NewLogon!AccountName" format="jsonf")
    property(outname="newlogonaccountdomain" name="$!win!NewLogon!AccountDomain" format="jsonf")
    property(outname="newlogonlogonid" name="$!win!NewLogon!LogonID" format="jsonf")
    property(outname="linkedlogonid" name="$!win!NewLogon!LinkedLogonID" format="jsonf")
    property(outname="networkaccountname" name="$!win!NewLogon!NetworkAccountName" format="jsonf")
    property(outname="logonguid" name="$!win!NewLogon!LogonGUID" format="jsonf")

    # Process fields
    property(outname="processid" name="$!win!Process!ProcessID" format="jsonf")
    property(outname="processname" name="$!win!Process!ProcessName" format="jsonf")
    property(outname="processcommandline" name="$!win!Process!ProcessCommandLine" format="jsonf")
    property(outname="tokenelevationtype" name="$!win!Process!TokenElevationType" format="jsonf")
    property(outname="mandatorylabel" name="$!win!Process!MandatoryLabel" format="jsonf")

    # Network fields
    property(outname="workstationname" name="$!win!Network!WorkstationName" format="jsonf")
    property(outname="sourcenetworkaddress" name="$!win!Network!SourceNetworkAddress" format="jsonf")
    property(outname="sourceport" name="$!win!Network!SourcePort" format="jsonf")

    # DetailedAuthentication fields
    property(outname="logonprocess" name="$!win!DetailedAuthentication!LogonProcess" format="jsonf")
    property(outname="authenticationpackage" name="$!win!DetailedAuthentication!AuthenticationPackage" format="jsonf")
    property(outname="transitedservices" name="$!win!DetailedAuthentication!TransitedServices" format="jsonf")
    property(outname="packagename" name="$!win!DetailedAuthentication!PackageName" format="jsonf")
    property(outname="keylength" name="$!win!DetailedAuthentication!KeyLength" format="jsonf")

    # Privileges fields
    property(outname="privilegelist" name="$!win!Privileges!PrivilegeList" format="jsonf")
}

# Template for basic field extraction (for comparison)
template(name="basicfmt" type="list") {
    property(name="$!win!Event!EventID")
    constant(value=",")
    property(name="$!win!Event!Channel")
    constant(value=",")
    property(name="$!win!Event!EventType")
    constant(value=",")
    property(name="$!win!Event!CategoryText")
    constant(value=",")
    property(name="$!win!Event!Computer")
    constant(value=",")
    property(name="$!win!Event!Provider")
    constant(value="\n")
}

ruleset(name="winsec") {
    action(type="mmsnareparse")
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'.json" template="jsonfmt")
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'.basic" template="basicfmt")
}

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="winsec")
'

startup_vg
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port

echo "Using Windows 2022 sample data..."
tcpflood -m 1 -I ${srcdir}/testsuites/mmsnareparse/sample-windows2022-security.data

echo "Using Windows 2025 sample data..."
tcpflood -m 1 -I ${srcdir}/testsuites/mmsnareparse/sample-windows2025-security.data

echo "Using sample events with detailed field information..."
tcpflood -m 1 -I ${srcdir}/testsuites/mmsnareparse/sample-events.data

shutdown_when_empty
wait_shutdown_vg
check_exit_vg

# spot-check that parsing completed by verifying representative output
data_point() {
    content_check "$1" "$RSYSLOG_OUT_LOG.$2"
}

data_point '4624,Security,Success Audit,Logon,WIN-5SB1I3G0V7U,Microsoft-Windows-Security-Auditing' basic

data_point '"eventid":"4624"' json

data_point '"privilegelist":"SeAssignPrimaryTokenPrivilege' json

exit_test
