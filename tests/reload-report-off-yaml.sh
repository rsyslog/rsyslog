#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML twin of reload-report-off.sh, with the setting given explicitly as
# global: config.reloadOnHUP: "off" in the YAML config.
export RR_FRONTEND=yaml
export RR_MODE=off
. ${srcdir:=.}/reload-report-off.sh
