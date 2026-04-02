#!/bin/bash
export USE_VALGRIND="YES"
export TB_TEST_MAX_RUNTIME=1500
. ${srcdir:-.}/rscript_http_request.sh
