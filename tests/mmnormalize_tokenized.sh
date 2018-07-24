#!/bin/bash
# added 2014-11-03 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mmnormalize_tokenized.sh\]: test for mmnormalize tokenized field_type
. $srcdir/diag.sh init
generate_conf
add_conf '
template(name="ips" type="string" string="%$.ips%\n")

template(name="paths" type="string" string="%$!fragments% %$!user%\n")
template(name="numbers" type="string" string="nos: %$!some_nos%\n")

module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514")

action(type="mmnormalize" rulebase=`echo $srcdir/testsuites/mmnormalize_tokenized.rulebase`)
if ( $!only_ips != "" ) then {
  set $.ips = $!only_ips;
  action(type="omfile" file="./rsyslog.out.log" template="ips")
} else if ( $!local_ips != "" ) then {
  set $.ips = $!local_ips;
  action(type="omfile" file="./rsyslog.out.log" template="ips")
} else if ( $!external_ips != "" ) then {
  set $.ips = $!external_ips;
  action(type="omfile" file="./rsyslog.out.log" template="ips")
} else if ( $!some_nos != "" ) then { 
  action(type="omfile" file="./rsyslog.out.log" template="numbers")
} else {
  action(type="omfile" file="./rsyslog.out.log" template="paths")
}
'
startup
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/tokenized_input
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
cp rsyslog.out.log /tmp/
. $srcdir/diag.sh content-check '[ "10.20.30.40", "50.60.70.80", "90.100.110.120", "130.140.150.160" ]'
. $srcdir/diag.sh content-check '[ "192.168.1.2", "192.168.1.3", "192.168.1.4" ]'
. $srcdir/diag.sh content-check '[ "10.20.30.40", "50.60.70.80", "190.200.210.220" ]'
. $srcdir/diag.sh content-check '[ "\/bin", "\/usr\/local\/bin", "\/usr\/bin" ] foo'
. $srcdir/diag.sh content-check '[ [ [ "10" ] ], [ [ "20" ], [ "30", "40", "50" ], [ "60", "70", "80" ] ], [ [ "90" ], [ "100" ] ] ]'
exit_test
