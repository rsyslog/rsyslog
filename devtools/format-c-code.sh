#!/bin/bash
#
# format-c-code.sh: Converts all tabs to spaces for .c and .h files.
#
# WARNING: This script modifies files in place and creates a commit.
# Ensure your working directory is clean and you have backed up any
# uncommitted work.

set -e # Exit immediately if a command exits with a non-zero status.

# --- Configuration ---
TAB_WIDTH=4
COMMIT_MESSAGE="style: Convert all tabs to spaces for .c and .h files"

# --- Main Script ---

echo "Searching for .c and .h files to reformat..."

# Use 'find' with '-print0' and 'xargs -0' to handle filenames with spaces
find . -type f \( -name "*.c" -o -name "*.h" \) -print0 | while IFS= read -r -d $'\0' file; do
    echo "Formatting $file"
    # Use a temporary file to be safe
    expand -t "$TAB_WIDTH" "$file" > "$file.tmp" && mv "$file.tmp" "$file"
done

echo "Formatting complete."
