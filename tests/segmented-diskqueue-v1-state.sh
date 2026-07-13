#!/bin/bash
# Experimental v1-state wrapper for explicit incompatibility coverage.
export STATE_MUTATION=v1
. ${srcdir:=.}/testsuites/segmented-diskqueue-invalid-state-driver.sh
