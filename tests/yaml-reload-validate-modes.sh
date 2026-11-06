#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Reject unsupported startup modes through the YAML global parameter frontend.
export RELOAD_YAML=1
. ${srcdir:=.}/reload-validate-modes.sh
