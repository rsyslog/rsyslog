#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Run the complete-event persistence oracle through a FixedArray segmented DA
# child, complementing the LinkedList wrapper with identical typed JSON,
# message-variable, local-variable, and RFC5424 field comparisons.
export SEGDISK_EVENT_DA=1
export SEGDISK_DA_QUEUE_TYPE=FixedArray
. ${srcdir:=.}/segmented-diskqueue-event-properties.sh
