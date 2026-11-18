#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run the delayed permitted-peer regression through the YAML configuration path.
export YAML_MODE=1
. ${srcdir:=.}/imdtls-permittedpeer-late-handshake.sh
