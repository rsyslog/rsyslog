#!/bin/bash
# Verification test for issue #4906: sparseArray lookup with ipv42num
# This checks if the proposed documentation example strategy actually works.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
lookup_table(name="ip_lookup" file="'$RSYSLOG_DYNNAME'.lookup_sparse_array_ipv4.lkp_tbl")

template(name="outfmt" type="string" string="%msg%: %$.lkp%\n")

# Assuming msg contains the IP address
set $.ip_num = ipv42num($msg);
set $.lkp = lookup("ip_lookup", $.ip_num);

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
cp -f $srcdir/testsuites/lookup_sparse_array_ipv4.lkp_tbl $RSYSLOG_DYNNAME.lookup_sparse_array_ipv4.lkp_tbl
startup

inject_ip() {
    injectmsg_literal "<13>1 2025-01-01T00:00:00Z localhost mytag - - - $1"
}

# 10.0.0.0 -> NetA
inject_ip "10.0.0.0"
# 10.0.0.5 -> NetA (inside range)
inject_ip "10.0.0.5"
# 10.0.0.255 -> NetA (end of range)
inject_ip "10.0.0.255"

# 10.0.1.0 -> Gap (explicitly defined)
inject_ip "10.0.1.0"
# 10.0.1.5 -> Gap (inside gap)
inject_ip "10.0.1.5"

# 10.0.2.0 -> NetB
inject_ip "10.0.2.0"
# 10.0.2.100 -> NetB
inject_ip "10.0.2.100"

# 9.0.0.0 -> Should be nomatch (less than first entry)
inject_ip "9.0.0.0"

shutdown_when_empty
wait_shutdown

content_check "10.0.0.0: NetA"
content_check "10.0.0.5: NetA"
content_check "10.0.0.255: NetA"
content_check "10.0.1.0: Gap"
content_check "10.0.1.5: Gap"
content_check "10.0.2.0: NetB"
content_check "10.0.2.100: NetB"
# For nomatch, lookup returns "nomatch" string if defined, or empty string?
# The table file doesn't define "nomatch". Default is empty string?
# Let's check documentation or code. 
# doc says: nomatch <string literal, default: "">
# So it should be empty.
content_check "9.0.0.0: "

exit_test
