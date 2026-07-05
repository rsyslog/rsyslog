#!/bin/bash
# Test for tocef() and cef_ext_escape() RainerScript functions.
# Verifies CEF header construction, header escaping, and extension
# value escaping per the CEF spec (Character Encoding section). The
# oracle compares the complete rendered output exactly so formatting,
# ordering, and escaping regressions cannot hide behind fragment matches.
# added 2026-05-30 by contributor
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

# tocef: basic output
set $!cef1 = tocef("0", "MyVendor", "rsyslog", "1.0", "100", "test event", "5", "src=10.0.0.1");

# tocef: pipe in vendor header field must be escaped as \|
set $!cef2 = tocef("0", "Vendor|X", "prod", "1.0", "100", "name", "3", "");

# tocef: backslash in product header field must be escaped by doubling it.
set $!cef3 = tocef("0", "V", "prod\\name", "1.0", "100", "name", "3", "");

# tocef: extensions appended verbatim
set $!cef4 = tocef("1", "V", "P", "2.0", "99", "n", "7", "src=1.2.3.4 dst=5.6.7.8 spt=514");

# tocef: eventclassid with =, %, # must be escaped (spec sec. "Header Field Definitions")
set $!cef6 = tocef("0", "V", "P", "1.0", "CVE=2021%1234#x", "vuln", "8", "");

# cef_ext_escape: backslash in a value must be escaped by doubling it.
set $!ext1 = cef_ext_escape("C:\\Windows\\file.txt");

# cef_ext_escape: equals sign in a value must become \=
set $!ext2 = cef_ext_escape("user=admin");

# cef_ext_escape: newline and CR must become literal \n and \r
set $!ext3 = cef_ext_escape("line1\nline2");
set $!ext4 = cef_ext_escape("line1\rline2");

# tocef combined with cef_ext_escape for a dynamic extension value
set $!cef5 = tocef("0", "V", "P", "1.0", "200", "file copy", "3",
                   "filePath=" & cef_ext_escape("C:\\dir\\file.txt") &
                   " msg=" & cef_ext_escape("status=ok"));

template(name="outfmt" type="string"
    string="%!cef1%\n%!cef2%\n%!cef3%\n%!cef4%\n%!ext1%\n%!ext2%\n%!ext3%\n%!ext4%\n%!cef5%\n%!cef6%\n")
local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m1 -y
shutdown_when_empty
wait_shutdown
export EXPECTED='CEF:0|MyVendor|rsyslog|1.0|100|test event|5|src=10.0.0.1
CEF:0|Vendor\|X|prod|1.0|100|name|3|
CEF:0|V|prod\\name|1.0|100|name|3|
CEF:1|V|P|2.0|99|n|7|src=1.2.3.4 dst=5.6.7.8 spt=514
C:\\Windows\\file.txt
user\=admin
line1\nline2
line1\rline2
CEF:0|V|P|1.0|200|file copy|3|filePath=C:\\dir\\file.txt msg=status\=ok
CEF:0|V|P|1.0|CVE\=2021\%1234\#x|vuln|8|'
cmp_exact $RSYSLOG_OUT_LOG
exit_test
