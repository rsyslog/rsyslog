#!/bin/bash
#
# devtools/format-code.sh
#
# This script formats all .c and .h files in the rsyslog source tree
# using clang-format, applying the style defined in the .clang-format file.
# It's intended to enforce the canonical code format for the repository.
#
# Usage:
#   ./devtools/format-code-exec.sh [-h]
#
# Options:
#   -h, --help    Display this help message and exit.
#
# Description:
#   This script performs an in-place reformatting of all C source (.c) and
#   header (.h) files within the current directory and its subdirectories.
#   It utilizes 'clang-format' and expects a '.clang-format' configuration
#   file to be present in the project root or a parent directory to define
#   the desired coding style.
#
#   This version uses 'find -exec ... +' for efficient processing of multiple
#   files per 'clang-format' invocation. This approach is generally faster
#   and more robust for large codebases. Any clang-format errors for individual
#   files will be printed directly by clang-format to stderr. The script's
#   final exit code will reflect if the overall formatting process encountered
#   a critical error.
#
#   Crucially, this version correctly groups the file name conditions using
#   parentheses `\( ... \)` to ensure that `clang-format` is executed for
#   both `.c` and `.h` files as intended.
#
#   Before running, ensure 'clang-format-18' is installed on your system.
#   It is highly recommended to commit your current changes or create a backup
#   before executing this script, as it modifies files directly.
#
# Exit Codes:
#   0: Overall formatting process completed successfully.
#   1: 'clang-format-18' is not found or not executable.
#   2: A critical error occurred during the 'find -exec' command.

# --- Script Start ---

# Set sensible defaults for shell options
set -euo pipefail # Exit on error, unset variables, and pipefail

# --- Configuration ---
readonly CLANG_FORMAT="clang-format-18"  # Specify the clang-format version to use

# --- Functions ---

# Function to display help message
show_help() {
  grep '^# ' "$0" | cut -c 3- | sed -n '/^Usage:/,/^Exit Codes:/p'
  exit 0
}

# --- Argument Parsing ---
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      show_help
      ;;
    *)
      echo "Error: Unknown option '$1'" >&2
      show_help
      ;;
  esac
  shift
done

# --- Pre-checks ---

# Check if clang-format is installed and executable
if ! command -v "$CLANG_FORMAT" &> /dev/null; then
  echo "Error: '$CLANG_FORMAT' command not found." >&2
  echo "Please install it. On Ubuntu, you can run: sudo apt install $CLANG_FORMAT" >&2
  exit 1
fi

# Check for .clang-format file
if ! find . -maxdepth 2 -name ".clang-format" -print -quit | grep -q .; then
  echo "Warning: No '.clang-format' file found in the current directory or its parent." >&2
  echo "Using clang-format's default style. It's highly recommended to create one for consistent formatting." >&2
  echo "You can generate a basic one with: clang-format -style=LLVM -dump-config > .clang-format" >&2
  echo ""
fi

echo "Starting code formatting for .c and .h files using 'find -exec ... +'..."
echo "Using $CLANG_FORMAT -i -style=file"
echo "This may take a moment. Any clang-format errors for individual files will be printed directly."
echo "Note: '$CLANG_FORMAT' only modifies files that deviate from the specified style."
echo ""

# --- Formatting Logic ---
# Find all .c and .h files recursively and execute clang-format on them.
# The '{} +' syntax passes multiple filenames to a single clang-format invocation,
# which is more efficient.
# The parentheses '\( ... \)' are crucial for correctly grouping the -name conditions.
if ! find . \( -name "*.c" -o -name "*.h" \) -exec "$CLANG_FORMAT" -i -style=file {} +; then
  echo "Error: The overall code formatting process failed." >&2
  echo "Please review the output above for any specific clang-format errors." >&2
  exit 2
fi

# Calculate total files found for summary
# Update this find command as well to use the correct parentheses for consistency.
TOTAL_FILES=$(find . \( -name "*.c" -o -name "*.h" \) | wc -l)

echo ""
echo "--- Formatting Summary ---"
echo "Total .c/.h files found and processed: $TOTAL_FILES"
echo "Code formatting completed successfully."
echo "The number of files actually changed depends on their adherence to the style."
echo "Please review changes using 'git diff' if in a Git repository."

# --- Script End ---

