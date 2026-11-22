#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML twin of reload-report-on-unsupported.sh (global: config.reloadOnHUP in
# a YAML config; same oracle). Registered under HAVE_LIBYAML.
export RR_FRONTEND=yaml
. ${srcdir:=.}/reload-report-on-unsupported.sh
