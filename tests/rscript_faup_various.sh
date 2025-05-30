#!/bin/bash
# added 2020-08-16 by Julien Thomas, released under ASL 2.0

source "${srcdir:=.}/diag.sh" init
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debug"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../contrib/ffaup/.libs/ffaup")
input(type="imtcp" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.tcpflood_port")
template(name="outfmt" type="string" string="%msg% %$.ret1% %$.faup% %$.ret2% %$.scheme% %$.ret3% %$.domain_without_tld% %$.ret4% %$.fragment% %$.ret5% %$.resource_path%\n")

if (not($msg contains "msgnum:")) then
    stop

set $!url = "https://www.rsyslog.com/doc/v8-stable/rainerscript/functions/mo-faup.html";

set $.faup = faup($!url);
set $.ret1 = script_error();
set $.scheme = faup_scheme($!url);
set $.ret2 = script_error();
set $.domain_without_tld = faup_domain_without_tld($!url);
set $.ret3 = script_error();
set $.fragment = faup_fragment($!url);
set $.ret4 = script_error();
set $.resource_path = faup_resource_path($!url);
set $.ret5 = script_error();
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m 5
wait_file_lines "$RSYSLOG_OUT_LOG" 5 60
shutdown_when_empty
wait_shutdown

# this test may need changes to produce a more deterministic
# output by sorting keys
EXPECTED=' msgnum:00000000: 0 { "scheme": "https", "credential": "", "subdomain": "www", "domain": "rsyslog.com", "domain_without_tld": "rsyslog", "host": "www.rsyslog.com", "tld": "com", "port": "", "resource_path": "\/doc\/v8-stable\/rainerscript\/functions\/mo-faup.html", "query_string": "", "fragment": "" } 0 https 0 rsyslog 0  0 /doc/v8-stable/rainerscript/functions/mo-faup.html
 msgnum:00000001: 0 { "scheme": "https", "credential": "", "subdomain": "www", "domain": "rsyslog.com", "domain_without_tld": "rsyslog", "host": "www.rsyslog.com", "tld": "com", "port": "", "resource_path": "\/doc\/v8-stable\/rainerscript\/functions\/mo-faup.html", "query_string": "", "fragment": "" } 0 https 0 rsyslog 0  0 /doc/v8-stable/rainerscript/functions/mo-faup.html
 msgnum:00000002: 0 { "scheme": "https", "credential": "", "subdomain": "www", "domain": "rsyslog.com", "domain_without_tld": "rsyslog", "host": "www.rsyslog.com", "tld": "com", "port": "", "resource_path": "\/doc\/v8-stable\/rainerscript\/functions\/mo-faup.html", "query_string": "", "fragment": "" } 0 https 0 rsyslog 0  0 /doc/v8-stable/rainerscript/functions/mo-faup.html
 msgnum:00000003: 0 { "scheme": "https", "credential": "", "subdomain": "www", "domain": "rsyslog.com", "domain_without_tld": "rsyslog", "host": "www.rsyslog.com", "tld": "com", "port": "", "resource_path": "\/doc\/v8-stable\/rainerscript\/functions\/mo-faup.html", "query_string": "", "fragment": "" } 0 https 0 rsyslog 0  0 /doc/v8-stable/rainerscript/functions/mo-faup.html
 msgnum:00000004: 0 { "scheme": "https", "credential": "", "subdomain": "www", "domain": "rsyslog.com", "domain_without_tld": "rsyslog", "host": "www.rsyslog.com", "tld": "com", "port": "", "resource_path": "\/doc\/v8-stable\/rainerscript\/functions\/mo-faup.html", "query_string": "", "fragment": "" } 0 https 0 rsyslog 0  0 /doc/v8-stable/rainerscript/functions/mo-faup.html'
cmp_exact "$RSYSLOG_OUT_LOG"
exit_test
