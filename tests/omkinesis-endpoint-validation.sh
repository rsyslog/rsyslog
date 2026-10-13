#!/bin/bash
# Copyright 2026 Adiscon GmbH.
# This file is part of the rsyslog project, released under ASL 2.0.
# Validate the endpoint security boundary and action record-size range.
# A valid loopback config must pass -N1; user-info URLs that conceal a remote
# host and out-of-range sizes must fail -N1 before any request is sent.
. ${srcdir:=.}/diag.sh init
require_plugin omkinesis

export AWS_ACCESS_KEY_ID=mock-access
export AWS_SECRET_ACCESS_KEY=mock-secret

conf="${RSYSLOG_DYNNAME}.kinesis-validation.conf"
check_config() {
    local expected="$1" endpoint="$2" limit="$3"
    cat >"$conf" <<EOF
module(load="../plugins/omkinesis/.libs/omkinesis")
action(type="omkinesis" stream="test-stream" region="us-east-1"
       partition_key="test-partition" endpoint="$endpoint"
       max_record_size="$limit")
EOF
    if ../tools/rsyslogd -N1 -f"$conf" -M../runtime/.libs:../.libs >/dev/null 2>&1; then
        [[ "$expected" == valid ]] || error_exit 1
    else
        [[ "$expected" == invalid ]] || error_exit 1
    fi
}

check_config valid 'http://127.0.0.1:12345/' 1048576
check_config invalid 'http://localhost:80@evil.example/' 1048576
check_config invalid 'http://127.0.0.1:80@evil.example/' 1048576
check_config invalid 'http://127.0.0.1:12345/' 10485761
exit_test
