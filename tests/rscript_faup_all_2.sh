#!/bin/bash
# added 2021-11-05 by Theo Bertin, released under ASL 2.0

source "${srcdir:=.}/diag.sh" init
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debug"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../contrib/ffaup/.libs/ffaup")
input(type="imtcp" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.tcpflood_port")
template(name="outfmt" type="string" string="%msg% %$.ret% %$.faup%\n")

if (not($msg contains "msgnum:")) then
    stop

set $!url = "https://user:pass@www.rsyslog.com:443/doc/v8-stable/rainerscript/functions/mo-faup.html?param=value#faup";

set $.faup = faup($!url);
set $.ret = script_error();
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m 3
wait_file_lines "$RSYSLOG_OUT_LOG" 3 60
shutdown_when_empty
wait_shutdown

# this test may need changes to produce a more deterministic
# output by sorting keys
EXPECTED=' msgnum:00000000: 0 { "scheme": "https", "credential": "user:pass", "subdomain": "www", "domain": "rsyslog.com", "domain_without_tld": "rsyslog", "host": "www.rsyslog.com", "tld": "com", "port": "443", "resource_path": "\/doc\/v8-stable\/rainerscript\/functions\/mo-faup.html", "query_string": "?param=value", "fragment": "#faup" }
 msgnum:00000001: 0 { "scheme": "https", "credential": "user:pass", "subdomain": "www", "domain": "rsyslog.com", "domain_without_tld": "rsyslog", "host": "www.rsyslog.com", "tld": "com", "port": "443", "resource_path": "\/doc\/v8-stable\/rainerscript\/functions\/mo-faup.html", "query_string": "?param=value", "fragment": "#faup" }
 msgnum:00000002: 0 { "scheme": "https", "credential": "user:pass", "subdomain": "www", "domain": "rsyslog.com", "domain_without_tld": "rsyslog", "host": "www.rsyslog.com", "tld": "com", "port": "443", "resource_path": "\/doc\/v8-stable\/rainerscript\/functions\/mo-faup.html", "query_string": "?param=value", "fragment": "#faup" }'
cmp_exact "$RSYSLOG_OUT_LOG"
exit_test
