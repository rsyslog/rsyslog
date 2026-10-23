#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Apply the exact deterministic random cap oracle through the YAML frontend.
export RANDOM_LIMIT_YAML=1
. ${srcdir:=.}/rscript-random-platform-limit.sh
