#!/bin/bash
# added 2020-01-12 by frikilax
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
source ${srcdir:-.}/impcap.sh

generate_conf
add_conf '
module(load="../contrib/impcap/.libs/impcap")

template(name="impcap" type="string" string="%$!impcap%\n")

input(type="impcap" file="'${srcdir}/dns.pcap'")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="impcap")
'

startup

shutdown_when_empty
wait_shutdown

# Check for the intended parsing of the packets in dns.cap
content_check '{ "ID": 24, "timestamp": "2005-03-30T08:52:17.733384+00:00", "net_bytes_total": 115, "ETH_src": "0:c0:9f:32:41:8c", "ETH_dst": "0:e0:18:b1:c:ad", "ETH_type": 2048, "ETH_typestr": "IP", "net_dst_ip": "192.168.170.8", "net_src_ip": "192.16
8.170.20", "IP_ihl": 5, "net_ttl": 128, "IP_proto": 17, "net_src_port": 53, "net_dst_port": 32795, "UDP_Length": 81, "UDP_Checksum": 52068, "DNS_transaction_id": 65251, "DNS_response_flag": true, "DNS_opcode": 0, "DNS_rcode": 0, "DNS_err
or": "NoError", "DNS_QDCOUNT": 1, "DNS_ANCOUNT": 2, "DNS_NSCOUNT": 0, "DNS_ARCOUNT": 0, "DNS_Names": [ { "qname": "www.isc.org", "qtype": 255, "type": "*", "qclass": 1, "class": "IN" } ], "net_bytes_data": 73 }'

content_count_check '{ "ID": ' 38

exit_test
