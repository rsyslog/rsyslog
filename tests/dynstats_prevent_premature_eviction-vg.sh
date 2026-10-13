#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the same synchronized survivor-table oracle under Valgrind,
# including every content and exact counter-sum assertion in the base test.
export USE_VALGRIND="YES"
. "${srcdir:-.}/dynstats_prevent_premature_eviction.sh"
