#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run automatic dynamic-file directory creation with segmentedDisk.
export QUEUE_TYPE=segmentedDisk
. ${srcdir:=.}/dircreate_dflt.sh
