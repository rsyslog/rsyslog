#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# maxBytes=100 holds one row per request, so every row after the first is
# sent by an early flush inside doAction(). A rejected flush is resent one
# row at a time inside the module, so only the bad row is dropped and every
# other message is inserted exactly once.
# This file is part of the rsyslog project, released under ASL 2.0
export CH_MAXBYTES=100
. ${srcdir:-.}/clickhouse-http-poison-row.sh
