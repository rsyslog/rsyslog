#!/bin/bash
#
# A simple wrapper to execute the rsyslog style checker python script.
# It passes all command-line arguments directly to the script.
#

# Find the directory this script is in, to robustly locate the .py file
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Execute the python script, passing along all arguments (e.g., a path)
"$SCRIPT_DIR/rsyslog_stylecheck.py"  --permit-empty-tab-line "$@"
