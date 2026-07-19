#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Crash immediately after the durable empty-state publication.
export SEGDISK_IDLE_FAULT_POINT=idle-empty-published
. ${srcdir:=.}/testsuites/segmented-da-idle-crash-driver.sh
