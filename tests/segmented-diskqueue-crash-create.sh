#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Crash after reserved segment creation; restart must adopt it safely.
export SEGDISK_FAULT_POINT=segment-created
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
