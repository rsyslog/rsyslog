# pmpanngfw

Module to detect and transform PAN-OS NGFW logs into a format compatible with mmnormalize

## How it works

Original log:

    Apr 10 02:48:29 1,2012/04/10 02:48:29,001606001116,THREAT,url,1,2012/04/10 02:48:28,##IP##,##IP##,##IP##,##IP##,rule1,counselor,,facebook-    base,vsys1,trust,untrust,ethernet1/2,ethernet1/1,forwardAll,2012/04/10 02:48:28,27555,1,8450,80,0,0,0x208000,tcp,alert,"www.facebook.    com/ajax/pagelet/generic.php/ProfileTimelineSectionPagelet?__a=1&ajaxpipe=1&ajaxpipe_token=AXgw4BUd5yCuD2rQ&data={""profile_id"":603261604,""start"":1333263600,""end"":1335855599,""query_type"":5,""page_index"":1,""section_container_id"":""ucp7d6_26"",""section_pagelet_id"":""pagelet_timeline_recent"",""unit_container_id"":""ucp7d6_25"",""current_scrubber_key"":""recent"",""time_cutoff"":null,""buffer"":1300,""require_click"":false,""num_visible_units"":5,""remove_dupes"":true}&__user=857280013&__adt=3&__att=iframe",(9999),social-networking,informational,client-to-server,0,0x0,192.168.0.0-192.168.255.255,United States,0,text/html

Transformed:

    Apr 10 02:48:29 1<tab>2012/04/10 02:48:29<tab>001606001116<tab>THREAT<tab>url<tab>1<tab>2012/04/10 02:48:28<tab>##IP##<tab>##IP##<tab>##IP##<tab>##IP##<tab>rule1<tab>counselor<tab><tab>facebook-base<tab>vsys1<tab>trust<tab>untrust<tab>ethernet1/2<tab>ethernet1/1<tab>forwardAll  2012/04/10 02:48:28 27555<tab>1<tab>8450<tab>80<tab>0<tab>0x208000<tab>tcp alert<tab>www.facebook.com/ajax/pagelet/generic.php/ProfileTimelineSectionPagelet?__a=1&ajaxpipe=1&ajaxpipe_token=AXgw4BUd5yCuD2rQ&data={"profile_id":603261604,"start":1333263600,"end":1335855599,"query_type":5,"page_index":1,"section_container_id":"ucp7d6_26","section_pagelet_id":"pagelet_timeline_recent","unit_container_id":"ucp7d6_25","current_scrubber_key":"recent","time_cutoff":null,"buffer":1300,"require_click":false,"num_visible_units":5,"remove_dupes":true}&__user=857280013&__adt=3&__att=iframe<tab>(9999)<tab>social-networking<tab>informational<tab>client-to-server<tab>0<tab>0x0<tab>192.168.0.0-192.168.255.255<tab>United States<tab>0<tab>text/html

## How to compile

    $ autoreconf -fvi
    $ ./configure --enable-pmpanngfw
    $ cd contrib/pmpanngfw/
    $ make

The resulting plugin should be found in contrib/pmpanngfw/.libs/

## Example config

    module(load="imtcp")
    module(load="pmpanngfw")
    module(load="mmnormalize")
    module(load="omrabbitmq")
    
    template(name="csv" type="list") {
        property(name="$!src_ip" format="csv")
        constant(value=",")
        property(name="$!dest_ip" format="csv")
        constant(value=",")
        property(name="$!url")
        constant(value="\n")
    }
    
    $template alljson,"%$!all-json%\n"
    
    template(name="mmeld_json" type="list") {
        constant(value="{")
        property(outname="@timestamp" name="timestamp" format="jsonf" dateFormat="rfc3339")
        constant(value=",")
        property(outname="@source_host" name="source" format="jsonf")
        constant(value=",")
        property(outname="@message" name="msg" format="jsonf")
        constant(value=",")
        property(outname="@timegenerated" name="timegenerated" format="jsonf" dateFormat="rfc3339")
        constant(value=",")
        property(outname="@timereported" name="timereported" format="jsonf" dateFormat="rfc3339")
        constant(value=",")
        property(outname="src_ip" name="$!src_ip" format="jsonf")
        constant(value=",")
        property(outname="dest_ip" name="$!dest_ip" format="jsonf")
        constant(value=",")
        property(outname="url" name="$!url" format="jsonf")
        constant(value=",")
        property(outname="tags" name="$!event.tags" format="jsonf")
        constant(value="}")
        constant(value="\n")
    }

    ruleset(name="pan-ngfw" parser=["rsyslog.panngfw", "rsyslog.rfc5424", "rsyslog.rfc3164"]) {
        action(type="mmnormalize" rulebase="palo_alto_networks.rb" userawmsg="on")
        if strlen($!unparsed-data) == 0 then {
            if $!log_subtype == "url" then set $!url = $!misc;
            *.* action(type="omrabbitmq" 
                 host="localhost"
                 virtual_host="/"
                 user="guest"
                 password="guest"
                 exchange="mmeld-syslog"
                 routing_key=""
                 exchange_type="fanout"
                 template="alljson")
        }
        *.* stop
    }
    
    input(type="imtcp" port="13514" ruleset="pan-ngfw")

## mmnormalize rulebase

See https://gist.github.com/jtschichold/87f59b99d98c8eac1da5

