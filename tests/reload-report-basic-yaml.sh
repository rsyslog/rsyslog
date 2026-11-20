#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML twin of reload-report-basic.sh: same scenario and oracle, but the
# setting (global: config.reloadOnHUP), the include: list, the changed
# fragment and the glob-added file are YAML files, so the YAML loader's file
# and include-glob recording is what is exercised.
export RR_FRONTEND=yaml
. ${srcdir:=.}/reload-report-basic.sh
