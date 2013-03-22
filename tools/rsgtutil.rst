========
rsgtutil
========

-----------------------------------
Manage (GuardTime) Signed Log Files
-----------------------------------

:Author: Rainer Gerhards <rgerhards@adiscon.com>
:Date: 2013-03-22
:Manual section: 1

SYNOPSIS
========

::

   rsgtutil [OPTIONS] [FILE] ...


DESCRIPTION
===========

This tool performs various maintenance operations on signed log files.
It specifically supports the GuardTime signature provider.

The *rsgtutil* tool is the primary tool to verify log file signatures,
dump signature file contents and carry out other maintenance operations.
The tool offers different operation modes, which are selected via
command line options.

The processing of multiple files is permitted. Depending on operation
mode, either the signature file or the base log file must be specified.
Within a single call, only a single operations mode is permitted. To 
use different modes on different files, multiple calles, one for each
mode, must be made.

If no file is specified on the command line, stdin is used instead. Note
that not all operation modes support stdin.

OPTIONS
=======

-D, --dump
  Select "dump" operations mode.

-t, --verify
  Select "verify" operations mode.

-T, --detect-file-type
  Select "detect-file-type" operations mode.

-B, --show-sigblock-params
  Select "show-sigblock-params" operations mode.

-s, --show-verified
  Prints out information about correctly verified blocks (by default, only
  errors are printed).

-v, --verbose
  Select verbose mode. Most importantly, hashes and signatures are printed
  in full length (can be **very** lengthy) rather than the usual abbreviation.

-P <URL>, --publications-server <URL>
  Sets the publications server. If not set but required by the operation a
  default server is used. The default server is not necessarily optimal
  in regard to performance and reliability.


OPERATION MODES
===============

The operation mode specifies what exactly the tool does with the provided
files. The default operation mode is "dump", but this may change in the future.
Thus, it is recommended to always set the operations mode explicitely. If 
multiple operations mode are set on the command line, results are 
unpredictable.

dump
----

This dump a the TLV header.

EXIT CODES
==========

The command returns an exit code of 0 if everything went fine, and some 
other code in case of failures.


EXAMPLES
========

::

    rsgtutil --verify logfile

    This verifies the file "logfile" via its associated signature file
    "logfile.gtsig". If errors are detected, these are reported to stderr.
    Otherwise, rsgtutil terminates without messages.


::

    rsgtutil --dump logfile.gtsig

    This dumps the content of the signature file "logfile.gtsig". The
    actual log file is not being processed and does not even need to be
    present.

SEE ALSO
========
**rsyslogd(8)**

COPYRIGHT
=========

This page is part of the *rsyslog* project, and is available under
LGPLv2.
