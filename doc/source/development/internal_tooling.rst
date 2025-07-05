Internal tooling
================

The ``devtools`` directory contains small helpers used during development
and in continuous integration.  One of them is ``rsyslog_stylecheck.py``
which verifies adherence to the coding style.

Usage::

   python3 devtools/rsyslog_stylecheck.py [options] [files...]

When no file is specified the script recursively scans the current
working directory.  The ``--max-errors`` option controls how many errors
are displayed.  The default is 100 while ``all`` disables the limit.

The checker reports trailing whitespace, invalid indentation and lines
that exceed the allowed length.  It exits with a non-zero status when
violations are found.
