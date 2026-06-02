#!/usr/bin/env bash
# Valgrind companion for GitHub issue #4024. The missing-database path used to
# leave a partially-created worker instance behind, which could crash only after
# shutdown cleanup. Running the same scenario under Valgrind makes that oracle
# explicit for CI.
script_dir=$(dirname "$0")
: ${srcdir:=.}
export USE_VALGRIND="YES"
export RS_TESTBENCH_VALGRIND_EXTRA_OPTS="$RS_TESTBENCH_VALGRIND_EXTRA_OPTS --suppressions=$srcdir/libmaxmindb.supp"
. "$script_dir/mmdb-open-missing.sh"
