#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Run the bounded lazy-recovery oracle through an automatic segmented DA child;
# the shared driver verifies recovery-pending rescheduling, counters, and later
# producer delivery while the child owns an undiscovered corrupt range.
export SEGDISK_RECOVERY_DA=1
. ${srcdir:=.}/segmented-diskqueue-recovery-budget.sh
