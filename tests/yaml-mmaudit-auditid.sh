#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Apply the complete audit-ID regression through a YAML ruleset with the supported legacy mmaudit action.
export MMAUDIT_ID_YAML=YES
. ${srcdir:=.}/mmaudit-auditid.sh
