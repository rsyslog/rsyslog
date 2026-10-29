#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise explicit backslash delimiter and ordinary-delimiter escaping through
# the YAML frontend using the shared semantic JSON output oracle.
export MMLEEF_DELIMITER_YAML=YES
. ${srcdir:=.}/mmleefparse-delimiter-backslash.sh
