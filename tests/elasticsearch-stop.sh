#!/bin/bash
# This is not a real test, but a script to stop ElasticSearch when all tests
# are done (or for manual testing).
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
ensure_module_needs_testing elasticsearch
cleanup_elasticsearch
exit_test
