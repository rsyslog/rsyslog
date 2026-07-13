#!/bin/bash
# Missing-state wrapper for fail-fast offline-recovery coverage.
export STATE_MUTATION=missing
. ${srcdir:=.}/testsuites/segmented-diskqueue-invalid-state-driver.sh
