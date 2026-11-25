#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# YAML companion of omfile-dynafile-relative-root.sh: the dynafile template
# and the omfile action are defined through the YAML frontend; the oracle
# (rejection without -C, startup warning, single log, acceptance with -C) is
# unchanged.
export RELROOT_YAML=YES
. "${srcdir:=.}/omfile-dynafile-relative-root.sh"
