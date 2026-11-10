#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Apply the fixed-width empty/beyond-end oracle to native YAML templates.
export FIXEDWIDTH_YAML=1
. ${srcdir:=.}/template-fixedwidth-empty.sh
