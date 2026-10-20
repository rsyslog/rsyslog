#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# segmentedDisk wrapper for the backend-neutral main-queue scope driver.
export QUEUE_TYPE=segmentedDisk QUEUE_SCOPE=main
. ${srcdir:=.}/testsuites/pure-disk-queue-scope-driver.sh
