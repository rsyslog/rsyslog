#!/usr/bin/env python3
"""rsyslog_stylecheck.py - verify rsyslog source style

This helper is used by developers and the CI system to ensure that
all source files comply with the project's coding guidelines.  It
relies only on the Python standard library and requires Python 3.
"""

import argparse
import os
import sys
import errno

def check_file(filename, trailing, firstspace, maxlen,
               permit_empty_tab_line, max_msgs):
    """Check a single file for style issues.

    Returns ``True`` if any problems were found.
    """
    had_err = False
    msgs = 0
    try:
        with open(filename, 'r', newline='', encoding='latin-1') as fp:
            for ln_nbr, line in enumerate(fp, 1):
                if max_msgs is not None and msgs >= max_msgs:
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
                    is_problematic = line_content and line_content[-1].isspace()
                    is_allowed_single_tab = permit_empty_tab_line and line_content == '\t'
                    if is_problematic and not is_allowed_single_tab:
                        msg = 'trailing space' if line_content.endswith(' ') else 'trailing whitespace'
                        print(f"error: {filename}:{ln_nbr}: {msg} at end of line:\n{line}", file=sys.stderr)
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


def collect_files(targets, excluded_dirs, excluded_files, ignore):
    """Gather all source files from *targets* respecting filters."""
    collected = []
    for target in targets:
        if os.path.isfile(target):
            filename = os.path.basename(target)
            if not filename.endswith(('.c', '.h')):
                continue
            if filename in excluded_files:
                continue
            if ignore and filename == ignore:
                continue
            collected.append(target)
            continue

        for dirpath, dirnames, filenames in os.walk(target):
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
                if ignore and filename == ignore:
                    continue
                collected.append(os.path.join(dirpath, filename))
    return collected

def main():
    """
    Parses arguments, walks the directory tree, and checks files.
    """
    parser = argparse.ArgumentParser(
        description="Check rsyslog source files for style issues.",
        add_help=False,
    )
    parser.add_argument(
        'targets', nargs='*', default=['.'],
        help="Files or directories to check (default: current directory).",
    )
    parser.add_argument('-i', '--ignore', help="File name to ignore.")
    parser.add_argument('-w', '--disable-trailing-whitespace', action='store_false', dest='trailing')
    parser.add_argument('-f', '--disable-first-space', action='store_false', dest='firstspace')
    parser.add_argument('-l', '--set-maxlength', type=int, default=120, metavar='length')
    parser.add_argument(
        '-m', '--max-errors', default='100',
        help="Number of errors to display (default: 100, 'all' for no limit).",
    )
    parser.add_argument(
        '--permit-empty-tab-line',
        action='store_true',
        help="Permit lines that contain only a single tab character."
    )
    parser.add_argument('-h', '--help', action='help', default=argparse.SUPPRESS)
    args = parser.parse_args()

    if args.max_errors.lower() == 'all':
        max_errors = None
    else:
        try:
            max_errors = int(args.max_errors)
        except ValueError:
            print(f"error: invalid --max-errors value '{args.max_errors}'", file=sys.stderr)
            sys.exit(2)

    excluded_dirs = {os.path.normpath("tests/zstd")}
    excluded_files = {"grammar.c", "grammar.h", "lexer.c", "lexer.h", "config.h"}

    files_to_check = collect_files(
        args.targets, excluded_dirs, excluded_files, args.ignore
    )

    had_err_global = False
    for path in files_to_check:
        if check_file(
            path,
            args.trailing,
            args.firstspace,
            args.set_maxlength,
            args.permit_empty_tab_line,
            max_errors,
        ):
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
