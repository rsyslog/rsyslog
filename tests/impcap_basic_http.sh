#!/bin/bash
# added 2019-12-28 by frikilax
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
source ${srcdir:-.}/impcap.sh

generate_conf
add_conf '
module(load="../contrib/impcap/.libs/impcap")

template(name="impcap" type="string" string="%$!impcap%\n")

input(type="impcap" file="'${srcdir}/http.cap'")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="impcap")
'

startup

shutdown_when_empty
wait_shutdown

# Check for the (static) part of the intended parsing of the packet in http.cap
content_check ', "net_bytes_total": 511, "ETH_src": "0:a:95:67:49:3c", "ETH_dst": "0:c0:f0:2d:4a:a3", "ETH_type": 2048, "ETH_typestr": "IP", "net_dst_ip": "192.168.69.1", "net_src_ip": "192.168.69.2", "IP_ihl": 5, "net_ttl": 64, "IP_proto": 6, "net_src_port": 34059, "net_dst_port": 80, "TCP_seq_number": 2415239731, "TCP_ack_number": 2518192935, "net_flags": "PA", "HTTP_method": "GET", "HTTP_request_URI": "\/test\/ethereal.html", "HTTP_version": "HTTP\/1.1", "HTTP_header_fields": { "Host": "cerberus", "User-Agent": "Mozilla\/5.0 (X11; U; Linux ppc; rv:1.7.3) Gecko\/20041004 Firefox\/0.10.1", "Accept": "text\/xml,application\/xml,application\/xhtml+xml,text\/html;q=0.9,text\/plain;q=0.8,image\/png,*\/*;q=0.5", "Accept-Language": "en-us,en;q=0.5", "Accept-Encoding": "gzip,deflate", "Accept-Charset": "ISO-8859-1,utf-8;q=0.7,*;q=0.7", "Keep-Alive": "300", "Connection": "keep-alive", "Cookie": "FGNCLIID=05c04axp1yaqynldtcdiwis0ag1" }, "net_bytes_data": 445 }'

content_count_check '{ "ID": ' 10

exit_test
