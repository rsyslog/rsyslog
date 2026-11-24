#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0
# Verify that YAML LogPath stops reading on a fatal error using the shared test.
export IMKLOG_LOGPATH_YAML=YES
. "${srcdir:=.}/imklog-logpath-read-error.sh"
