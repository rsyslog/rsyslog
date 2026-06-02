#!/bin/bash
export IMBEATS_LIMITS_CHECK_STATS=YES
script_dir=${srcdir:-$(dirname "$0")}
. "$script_dir/imbeats-limits.sh"
