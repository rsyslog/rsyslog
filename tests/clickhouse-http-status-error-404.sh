#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# A missing table is HTTP 404 with X-ClickHouse-Exception-Code 60. It is not
# caused by the data, so the message is kept, retried, and inserted once the
# table exists, and nothing reaches the error file.
# This file is part of the rsyslog project, released under ASL 2.0
export CH_STATUS=404
export CH_CODE=60
export CH_BODY="Code: 60. DB::Exception: Table rsyslog.statuserror does not exist. (UNKNOWN_TABLE)"
. ${srcdir:-.}/clickhouse-http-status-error.sh
