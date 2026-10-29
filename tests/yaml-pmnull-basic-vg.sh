#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Check YAML built-in/configured pmnull defaults under Valgrind.
export USE_VALGRIND=YES
. ${srcdir:-.}/yaml-pmnull-basic.sh
