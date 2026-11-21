#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Apply the exact-payload and bounded-warning scenarios to the imptcp framer.
export LEGACY_WARNING_INPUT=imptcp
. "${srcdir:=.}/legacy-zlib-warnings.sh"
