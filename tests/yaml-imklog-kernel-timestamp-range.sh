#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0
# Verify the imklog kernel timestamp range checks with a YAML config, using
# the shared test.
export IMKLOG_KERNEL_TS_YAML=YES
. "${srcdir:=.}/imklog-kernel-timestamp-range.sh"
