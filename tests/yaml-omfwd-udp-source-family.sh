#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the same source-family delivery and suspension oracles with YAML
# sender configuration; the common test documents fixture and wait semantics.
export OMFWD_FAMILY_YAML=1
. ${srcdir:=.}/omfwd-udp-source-family.sh
