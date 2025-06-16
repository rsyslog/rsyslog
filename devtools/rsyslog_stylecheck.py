#!/usr/bin/env python3
import argparse
import os
import sys
import errno

def check_file(filename, trailing, firstspace, maxlen, permit_empty_tab_line):
    """
    Checks a single file for various style issues.
    """
    had_err = False
    msgs = 0
    max_msgs = 20
    try:
        with open(filename, 'r', newline='', encoding='latin-1') as fp:
            for ln_nbr, line in enumerate(fp, 1):
                if msgs >= max_msgs:
                    print(f"Reached max {max_msgs} messages, not reporting more", file=sys.stderr)
                    return had_err
                if not line.endswith('\n'):
                    print(f"error: {filename}:{ln_nbr}: line is missing LF:\n{line}", file=sys.stderr)
                    had_err = True
                    msgs += 1
                    continue

                line_content = line.rstrip('\r\n')

                # Max line length check
                pos = 0
                for char in line_content:
                    if char == '\t':
                        pos += 8 - (pos % 8)
                    else:
                        pos += 1
                if pos > maxlen:
                    print(f"error: {filename}:{ln_nbr}: line too long ({pos}):\n{line}", file=sys.stderr)
                    had_err = True
                    msgs += 1

                # Invalid indentation check
                if firstspace and line.startswith(' ') and not line.startswith(' *'):
                    print(f"error: {filename}:{ln_nbr}: invalid indention (SP at column 1):\n{line}", file=sys.stderr)
                    had_err = True
                    msgs += 1

                # Trailing whitespace check
                if trailing:
                    is_problematic_trailing_space = (line_content and line_content[-1].isspace())
                    is_allowed_single_tab = (permit_empty_tab_line and line_content == '\t')

                    if is_problematic_trailing_space and not is_allowed_single_tab:
                        print(f"error: {filename}:{ln_nbr}: trailing whitespace at end of line:\n{line}", file=sys.stderr)
                        had_err = True
                        msgs += 1

    except FileNotFoundError:
        # Using errno.ENOENT is more portable and readable than a hardcoded '2'
        print(f"{filename}: {os.strerror(errno.ENOENT)}", file=sys.stderr)
        return True
    except Exception as e:
        print(f"An unexpected error occurred with file {filename}: {e}", file=sys.stderr)
        return True
    return had_err

def main():
    """
    Parses arguments, walks the directory tree, and checks files.
    """
    parser = argparse.ArgumentParser(
        description="Check rsyslog source files for style issues.", add_help=False
    )
    parser.add_argument('path', nargs='?', default='.', help="Directory to check (default: current directory).")
    parser.add_argument('-i', '--ignore', help="File name to ignore.")
    parser.add_argument('-w', '--disable-trailing-whitespace', action='store_false', dest='trailing')
    parser.add_argument('-f', '--disable-first-space', action='store_false', dest='firstspace')
    parser.add_argument('-l', '--set-maxlength', type=int, default=120, metavar='length')
    parser.add_argument(
        '--permit-empty-tab-line',
        action='store_true',
        help="Permit lines that contain only a single tab character."
    )
    parser.add_argument('-h', '--help', action='help', default=argparse.SUPPRESS)
    args = parser.parse_args()

    excluded_dirs = {os.path.normpath("tests/zstd")}
    excluded_files = {"grammar.c", "grammar.h", "lexer.c", "lexer.h", "config.h"}
    had_err_global = False

    for dirpath, dirnames, filenames in os.walk(args.path):
        dirnames[:] = [d for d in dirnames if not d.startswith('.')]
        normalized_dirpath = os.path.normpath(dirpath)
        if normalized_dirpath in excluded_dirs:
            dirnames[:] = []
            continue
        for filename in filenames:
            if not filename.endswith(('.c', '.h')):
                continue
            if filename in excluded_files:
                continue
            if args.ignore and filename == args.ignore:
                continue

            full_path = os.path.join(dirpath, filename)
            # Removed 'dos' argument from the call
            if check_file(full_path, args.trailing, args.firstspace,
                          args.set_maxlength, args.permit_empty_tab_line):
                had_err_global = True

    if had_err_global:
        doc_url = "https://www.rsyslog.com/doc/master/development/dev_codestyle.html"
        print('\n\n====================================================================', file=sys.stderr)
        print('Codestyle Error:', file=sys.stderr)
        print('Your code is not compliant with the rsyslog codestyle policies', file=sys.stderr)
        print(f'See here for more information: {doc_url}', file=sys.stderr)
    else:
        print('Codestyle check was successful!')

    sys.exit(1 if had_err_global else 0)

if __name__ == "__main__":
    main()
