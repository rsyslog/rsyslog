#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run the CEE/message-variable action-queue persistence regression unchanged
# against segmentedDisk.
export QUEUE_TYPE=segmentedDisk
. ${srcdir:=.}/cee_diskqueue.sh
