#!/bin/bash
# Crash after durable next-segment reservation; restart must not reuse its ID.
export SEGDISK_FAULT_POINT=reservation-published
. ${srcdir:=.}/testsuites/segmented-diskqueue-crash-driver.sh
