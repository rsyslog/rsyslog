#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run the FIFO output scenario with a segmentedDisk main queue.
export QUEUE_TYPE=segmentedDisk
. ${srcdir:=.}/pipeaction.sh
