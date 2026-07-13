#!/bin/bash
# Repeat reservation publication crashes three times to prove ID reconciliation
# remains idempotent across consecutive failures.
export SEGDISK_FAULT_POINT=reservation-published SEGDISK_FAULT_REPEATS=3
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
