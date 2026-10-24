#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH
#
# This file is part of rsyslog.
# Released under ASL 2.0

## Verify driver initialization/empty-input behavior and legacy-config reset.
## The state oracle compares the complete tracked legacy state from fresh
## A->A and A->B->A processes; matching exit codes alone are insufficient.

set -eu

if [ "$#" -ne 2 ]; then
	printf 'usage: %s DRIVER_REGRESSION STATE_REGRESSION\n' "$0" >&2
	exit 2
fi

driver_regression=$1
state_regression=$2

"$driver_regression"

aa_state=$("$state_regression" aa)
aba_state=$("$state_regression" aba)
if [ "$aa_state" != "$aba_state" ]; then
	printf 'legacy state mismatch: A->A=%s A->B->A=%s\n' \
		"$aa_state" "$aba_state" >&2
	exit 1
fi

printf 'legacy state reset regressions: PASS (%s)\n' "$aa_state"
