#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# ClickHouse reports a violated CHECK constraint with HTTP 500 and
# X-ClickHouse-Exception-Code 469. The code marks it as a data error, so only
# the offending row is dropped and later messages still flow.
# This file is part of the rsyslog project, released under ASL 2.0
export CH_REJECT_STATUS=500
export CH_REJECT_CODE=469
export CH_REJECT_BODY="Code: 469. DB::Exception: Constraint c_t for table rsyslog.poison is violated at row 1. (VIOLATED_CONSTRAINT)"
. ${srcdir:-.}/clickhouse-http-poison-row.sh
