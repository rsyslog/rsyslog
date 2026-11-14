#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Preserve the exact compatible-family delivery/count oracle with udp.sendToAll
# enabled for the mixed-family case; incompatible families must not cause retries.
export OMFWD_FAMILY_SEND_TO_ALL=on
export OMFWD_FAMILY_YAML=1
. ${srcdir:=.}/omfwd-udp-source-family.sh
