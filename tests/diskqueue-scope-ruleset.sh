#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Classic Disk wrapper for the backend-neutral ruleset-queue scope driver.
export QUEUE_TYPE=Disk QUEUE_SCOPE=ruleset
. ${srcdir:=.}/testsuites/pure-disk-queue-scope-driver.sh
