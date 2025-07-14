#!/usr/bin/env python3
import sys
import os

def fix_trailing_whitespace_chars(filepath):
    """
    Removes only trailing Space (SP), Horizontal Tab (HT), and Carriage Return (CR)
    characters from each line in a file, in-place. Line Feed (LF) and other
    newline characters (like Vertical Tab or Form Feed if they were somehow trailing)
    are preserved.

    Multiple SP/HT/CR characters at the end of a line will all be removed in one run.
    This function also handles line endings consistently and ensures the file's
    final newline status (present or absent) is preserved.

    :param filepath: The path to the file to be processed.
    :type filepath: str
    :return: A tuple containing (number_of_lines_fixed, success_boolean).
             Returns (0, False) if an error occurs.
    :rtype: tuple
    :raises FileNotFoundError: If the specified file does not exist.
    :raises IOError: If there is an issue reading from or writing to the file.
    :raises Exception: For any other unexpected errors during file processing.
    """
    lines_fixed_count = 0
    # Define the characters to be removed from the end of a line
    # This specifically targets Space, Horizontal Tab, and Carriage Return.
    CHARS_TO_STRIP = ' \t\r'

    try:
        with open(filepath, 'r', newline='') as f:
            original_lines = f.readlines()

        cleaned_lines = []
        for i, line in enumerate(original_lines):
            # Check if the line ends with a Line Feed (LF).
            # This is crucial for preserving actual newlines after stripping other whitespace.
            ends_with_lf = line.endswith('\n')

            # Temporarily remove the LF if present, then strip the desired characters.
            line_to_strip = line[:-1] if ends_with_lf else line
            cleaned_line_content = line_to_strip.rstrip(CHARS_TO_STRIP)

            # Re-add the LF if it was originally present
            cleaned_line = cleaned_line_content + '\n' if ends_with_lf else cleaned_line_content

            if cleaned_line != line:
                lines_fixed_count += 1
            cleaned_lines.append(cleaned_line)

        # Special handling for the very last line of the file:
        # If the original file did not end with a newline (LF), ensure the fixed version also does not.
        # This handles cases where the last line itself might have trailing SP/HT/CR but no LF.
        if original_lines and not original_lines[-1].endswith('\n') and cleaned_lines:
            # Re-apply the specific rstrip to the very last line in `cleaned_lines`
            # to guarantee its final state if it was modified and didn't end with LF.
            cleaned_lines[-1] = cleaned_lines[-1].rstrip(CHARS_TO_STRIP)

        # Only write back if changes were actually made to avoid unnecessary file writes.
        # This comparison handles cases where only trailing SP/HT/CR are removed.
        if lines_fixed_count > 0 or original_lines != cleaned_lines:
            with open(filepath, 'w', newline='') as f:
                f.writelines(cleaned_lines)
            return lines_fixed_count, True
        else:
            return 0, True # No fixes needed, but successful
    except FileNotFoundError:
        print(f"Error: File not found: {filepath}", file=sys.stderr)
        return 0, False
    except IOError as e:
        print(f"Error reading/writing file {filepath}: {e}", file=sys.stderr)
        return 0, False
    except Exception as e:
        print(f"An unexpected error occurred while processing {filepath}: {e}", file=sys.stderr)
        return 0, False

def main():
    """
    Main function to parse command-line arguments and process specified files.

    Expects file paths as command-line arguments. Each file will be
    processed to remove only trailing Space (SP), Horizontal Tab (HT), and Carriage Return (CR) characters.
    Outputs a summary of fixes.
    """
    if len(sys.argv) < 2:
        print("Usage: python fix_spaces.py <file1> [file2 ...]", file=sys.stderr)
        sys.exit(1)

    total_files_processed = 0
    total_lines_fixed = 0
    fixed_files_details = {} # Store {filepath: lines_fixed_count}

    for filepath in sys.argv[1:]:
        if os.path.isfile(filepath):
            total_files_processed += 1
            lines_fixed, success = fix_trailing_whitespace_chars(filepath)
            if success and lines_fixed > 0:
                fixed_files_details[filepath] = lines_fixed
                total_lines_fixed += lines_fixed
        else:
            print(f"Warning: Skipping '{filepath}' as it is not a valid file or does not exist.", file=sys.stderr)

    print("\n--- Summary ---")
    if fixed_files_details:
        print("Files fixed:")
        for filepath, count in fixed_files_details.items():
            print(f"  - {filepath}: {count} lines fixed")
    else:
        print("No files required fixing or no valid files were provided.")

    print(f"\nTotal files processed: {total_files_processed}")
    print(f"Total lines fixed across all files: {total_lines_fixed}")

if __name__ == "__main__":
    main()
