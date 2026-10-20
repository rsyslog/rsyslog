#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
export USE_VALGRIND="YES"
export TB_TEST_MAX_RUNTIME=1500
. ${srcdir:-.}/rscript_http_request.sh
