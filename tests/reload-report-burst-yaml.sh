#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML twin of reload-report-burst.sh (YAML settings, include: list and
# FIFO-blocked YAML include in the child; same oracle).
export RR_FRONTEND=yaml
. ${srcdir:=.}/reload-report-burst.sh
