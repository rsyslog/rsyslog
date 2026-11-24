#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# Verify that imklog's own diagnostic uses local0.err when
# internalMsgFacility is set through the YAML frontend.
export IMKLOG_INTERNALMSGFACILITY_YAML=YES
. "${srcdir:=.}/imklog-internalmsgfacility.sh"
