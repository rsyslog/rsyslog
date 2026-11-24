#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Experimental v1-state wrapper for explicit incompatibility coverage.
export STATE_MUTATION=v1
. ${srcdir:=.}/testsuites/segmented-diskqueue-invalid-state-driver.sh
