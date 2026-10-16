#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Native YAML parity for committed off not enabling itself from a later candidate.
export RELOAD_MODE_YAML=1
. "${srcdir:=.}/testsuites/reload-mode-off-driver.sh"
