#!/bin/bash
# added 2020-06-30 by frikilax
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
source ${srcdir:-.}/impcap.sh

generate_conf
add_conf '
module(load="../contrib/impcap/.libs/impcap")

template(name="impcap" type="string" string="%$!impcap%\n")

input(type="impcap" file="'${srcdir}/bug_ether.pcap'")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="impcap")
'

startup

shutdown_when_empty
wait_shutdown

# Check for the intended parsing of the packet in wrong_ethtype.cap
content_check '{ "ID": 1, "timestamp": "2020-06-30T17:07:25.980151+00:00", "net_bytes_total": 14, "ETH_src": "0:0:0:0:0:0", "ETH_dst": "ff:ff:ff:ff:ff:ff", "ETH_type": 36865, "ETH_typestr": "UNKNOWN", "net_bytes_data": 0 }'

exit_test
