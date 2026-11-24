#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Repeat the real-daemon timestamp regression with native YAML listener options.
export IMUXSOCK_TEST_YAML=yes
. "${srcdir:=.}/imuxsock-timestamp-rewrite.sh"
