========
rsgtutil
========

-----------------------------------
Manage (GuardTime) Signed Log Files
-----------------------------------

:Author: Rainer Gerhards <rgerhards@adiscon.com>
:Date: 2013-03-25
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

-e, --extend
  Select extend mode. This extends the RFC3161 signatures. Note that this
  mode also implies a full verification. If there are verify errors, extending
  will also fail.

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

The provided *signature* files are dumped. For each top-level record, the*u
type code is printed as well as q short description. If there is additional
information available, it will be printed in tab-indented lines below the
main record dump. The actual *log* files need not to be present.

verify
------

This mode does not work with stdin. On the command line, the *log* file names
are specified. The corresponding *signature* files (ending on ".gtsig") must also
be preset at the same location as the log file. In verify mode, both the log
and signature file is read and the validity of the log file checked. If verification
errors are detected these are printed and processing of the file aborted. By default,
each file is verified individually, without taking cross-file hash chains into
account (so the order of files on the command line does not matter).

Note that the actual amount of what can be verified depends on the parameters with
which the signature file was written. If record and tree hashes are present, they
will be verified and thus fine-granular error reporting is possible. If they are
not present, only the block signature itself is verified.

By default, only errors are printed. To also print successful verifications, use the
**--show-verified** option.


extend
------
This extends the RFC3161 signatures. This includes a full verification
of the file. If there are verification errors, extending will also fail.
Note that a signature can only be extended when the required hash has been
published. Currently, these hashes are created at the 15th of each month at
0:00hrs UTC. It takes another few days to get them finally published. As such,
it can be assumed that extending is only possible after this happend (which
means it may take slightly above a month).

To prevent data corruption, a copy of the signature file is created during
extension. So there must be enough disk space available for both files,
otherwise the operation will fail. If the log file is named logfile, the
signature file is logfile.gtsig and the temporary work file is named
logfile.gtsig.new. When extending finished successfully, the original
signature file (logfile.gtsig in our example) is renamed with the .old
postfix (logfile.gtsig.old) and the temporary file written under the
original name. The .old file can be deleted. It is just kept as a 
precaution to prevent signature loss. Note that any already existing
.old or .new files are overwritten by these operations.


detect-file-type
----------------
This mode is used to detect the type of some well-know files used inside the 
signature system. The detection is based on the file header. This mode is
primarily a debug aid.


show-sigblock-params
--------------------
This mode is used to print signature block parameters. It is similar to *dump*
mode, but will ignore everything except signature blocks. Also, some additional
meta information is printed. This mode is primarily a debug aid.

EXIT CODES
==========

The command returns an exit code of 0 if everything went fine, and some 
other code in case of failures.


EXAMPLES
========

**rsgtutil --verify logfile**

This verifies the file "logfile" via its associated signature file
"logfile.gtsig". If errors are detected, these are reported to stderr.
Otherwise, rsgtutil terminates without messages.

**rsgtutil --dump logfile.gtsig**

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
