#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# Without bulk mode every message is its own request, so a rejection is
# final in the module and must not stop later messages.
# This file is part of the rsyslog project, released under ASL 2.0
export CH_BULKMODE=off
. ${srcdir:-.}/clickhouse-http-poison-row.sh
