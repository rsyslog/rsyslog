#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# With no module default configured, implicit modern ompipe output must exactly
# match an explicit RSYSLOG_FileFormat action for the same injected message.
export OMPIPE_TEMPLATE_UNSET=YES
. ${srcdir:=.}/ompipe-module-template.sh
