#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# A bad row in a batch that was suspended first, as during a ClickHouse
# restart. The stub answers the first three inserts that hold message 1 with
# HTTP 503, then rejects message 1 with code 27 when the batch is retried. omclickhouse
# resends the rows one at a time itself, so the retried transaction ends with
# OK instead of a data failure that the engine's retry loop would post again
# without a pause. Every message is delivered or recorded; the wait times out
# if the worker loops or stalls.
# This file is part of the rsyslog project, released under ASL 2.0
export CH_OUTAGE=3
export CH_ALLOW_DUPLICATES=1
. ${srcdir:-.}/clickhouse-http-poison-row.sh
