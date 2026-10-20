#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Crash after durable next-segment reservation; restart must not reuse its ID.
export SEGDISK_FAULT_POINT=reservation-published
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
