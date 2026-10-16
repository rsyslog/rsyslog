#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify committed off cannot reread an on candidate; common driver documents the oracle.
unset RELOAD_MODE_YAML
. "${srcdir:=.}/testsuites/reload-mode-off-driver.sh"
