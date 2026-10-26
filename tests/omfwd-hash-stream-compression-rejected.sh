#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# pool.policy="hash" must be rejected with compression.mode="stream:always".
# A deferred send is repeated with the same bytes, but stream compression
# would deflate the same input again and offer different ones.
#
# Oracle: rsyslogd -N1 exits non-zero and the log contains the exact
# diagnostic below. A second -N1 with compression.mode="single" must
# succeed, proving the check is the stream restriction and not a blanket
# compression rejection.
. ${srcdir:=.}/diag.sh init

check_conf() {
    local label="$1"
    local mode="$2"
    generate_conf
    add_conf '
template(name="shardkey" type="string" string="constant-shard-key")

action(type="omfwd"
       target=["127.0.0.1", "127.0.0.1"]
       port=["1514", "1515"]
       protocol="tcp"
       compression.mode="'"$mode"'"
       pool.policy="hash"
       pool.hashkey="shardkey")
'
    ../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.${label}.log" 2>&1
}

check_conf stream 'stream:always'
if [ $? -eq 0 ]; then
    echo "FAIL: expected config validation failure for hash policy with stream compression"
    cat "${RSYSLOG_DYNNAME}.stream.log"
    error_exit 1
fi
content_check 'omfwd: pool.policy="hash" cannot be combined with compression.mode="stream:always"' \
    "${RSYSLOG_DYNNAME}.stream.log"

check_conf single 'single'
if [ $? -ne 0 ]; then
    echo "FAIL: compression.mode=single with pool.policy=hash was rejected"
    cat "${RSYSLOG_DYNNAME}.single.log"
    error_exit 1
fi

exit_test
