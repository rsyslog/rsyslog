#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exact native assertions and normal runtime cleanup are the oracle. No daemon
# workers or network listeners start; ordinary link closure resolves every symbol.
. "${srcdir:=.}/diag.sh" init
"${abs_top_builddir:-..}/tools/parser_policy_unit" || error_exit 1
exit_test
