#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Run the complete-event persistence oracle through a segmented DA child.
export SEGDISK_EVENT_DA=1
. ${srcdir:=.}/segmented-diskqueue-event-properties.sh
