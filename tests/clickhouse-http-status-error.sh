#!/bin/bash
# Regression coverage for GitHub issues #3485 and #3487. omclickhouse must
# classify HTTP error replies as ClickHouse request failures even when the
# response body does not match older DB::Exception string probes, and the
# failure must be visible through the normal rsyslog error path when no
# errorFile is configured. The oracle is the configured omfile destination
# after synchronized shutdown; the fake HTTP server avoids a live ClickHouse
# dependency for this error-detection path.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin omclickhouse
check_command_available python3

export NUMMESSAGES=1

omhttp_start_server 0 --fail-with-400-after 0

generate_conf
# omhttp_start_server sets omhttp_server_lstnport after the fake server binds.
# shellcheck disable=SC2154
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="outfmt" option.stdsql="on" type="string"
         string="INSERT INTO rsyslog.statuserror (id, severity, message) VALUES (%msg:F,58:2%, %syslogseverity%, '\''%msg%'\'')")

:syslogtag, contains, "tag" action(type="omclickhouse"
                                   server="localhost"
                                   port="'$omhttp_server_lstnport'"
                                   usehttps="off"
                                   bulkmode="off"
                                   template="outfmt")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
injectmsg
shutdown_when_empty
wait_shutdown
omhttp_stop_server

content_check "omclickhouse: ClickHouse request failed with HTTP status 400: BAD REQUEST"

exit_test
