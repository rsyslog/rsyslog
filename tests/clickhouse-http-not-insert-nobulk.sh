#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# Without bulk mode the row that is not an INSERT is the first request of the
# worker, rejected before any post set the request URL. It must still be
# recorded with an empty URL and dropped.
# This file is part of the rsyslog project, released under ASL 2.0
export CH_BULKMODE=off
. ${srcdir:-.}/clickhouse-http-not-insert.sh
