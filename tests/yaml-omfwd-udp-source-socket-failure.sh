#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Check the same explicit-source socket failure and valid delivery control with
# YAML action mappings; the common test documents the bounded exact oracle.
export OMFWD_FAILURE_YAML=1
. ${srcdir:=.}/omfwd-udp-source-socket-failure.sh
