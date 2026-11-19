#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0
# Valgrind variant of rscript-parse-error-noleak.sh: the same broken configs
# must not leak parser values or read freed lexer memory.
export USE_VALGRIND="YES"
. ${srcdir:=.}/rscript-parse-error-noleak.sh
