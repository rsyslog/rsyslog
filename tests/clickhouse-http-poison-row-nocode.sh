#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# A server or proxy that sends HTTP 400 without X-ClickHouse-Exception-Code or
# a "Code: N." body is still treated as a data error, as ClickHouse versions
# without the header were.
# This file is part of the rsyslog project, released under ASL 2.0
export CH_REJECT_STATUS=400
export CH_REJECT_CODE=none
export CH_REJECT_BODY="BAD REQUEST"
. ${srcdir:-.}/clickhouse-http-poison-row.sh
