******************************
imfile: Text File Input Module
******************************

.. index:: ! imfile

===========================  ===========================================================================
**Module Name:**             **imfile**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================

Purpose
=======

This module provides the ability to convert any standard text file
into a syslog
message. A standard text file is a file consisting of printable
characters with lines being delimited by LF.

The file is read line-by-line and any line read is passed to rsyslog's
rule engine. The rule engine applies filter conditions and selects which
actions needs to be carried out. Empty lines are **not** processed, as
they would result in empty syslog records. They are simply ignored.

As new lines are written they are taken from the file and processed.
Depending on the selected mode, this happens via inotify or based on
a polling interval. Especially in polling mode, file reading doesn't
happen immediately. But there are also slight delays (due to process
scheduling and internal processing) in inotify mode.

The file monitor supports file rotation. To fully work,
rsyslogd must run while the file is rotated. Then, any remaining lines
from the old file are read and processed and when done with that, the
new file is being processed from the beginning. If rsyslogd is stopped
during rotation, the new file is read, but any not-yet-reported lines
from the previous file can no longer be obtained.

When rsyslogd is stopped while monitoring a text file, it records the
last processed location and continues to work from there upon restart.
So no data is lost during a restart (except, as noted above, if the file
is rotated just in this very moment).

Notable Features
================

- :ref:`Metadata`
- :ref:`State-Files`
- :ref:`WildCards`
- presentation on `using wildcards with imfile <http://www.slideshare.net/rainergerhards1/using-wildcards-with-rsyslogs-file-monitor-imfile>`_


Configuration
=============

.. note::

  Parameter names are case-insensitive; CamelCase is recommended for
   readability.


Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imfile-mode`
     - .. include:: ../../reference/parameters/imfile-mode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-readtimeout`
     - .. include:: ../../reference/parameters/imfile-readtimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-deletestateonfilemove`
     - .. include:: ../../reference/parameters/imfile-deletestateonfilemove.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-timeoutgranularity`
     - .. include:: ../../reference/parameters/imfile-timeoutgranularity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-sortfiles`
     - .. include:: ../../reference/parameters/imfile-sortfiles.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-pollinginterval`
     - .. include:: ../../reference/parameters/imfile-pollinginterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-statefile-directory`
     - .. include:: ../../reference/parameters/imfile-statefile-directory.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-maxinotifywatches`
     - .. include:: ../../reference/parameters/imfile-maxinotifywatches.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-inotifyfallbackinterval`
     - .. include:: ../../reference/parameters/imfile-inotifyfallbackinterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imfile-file`
     - .. include:: ../../reference/parameters/imfile-file.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-tag`
     - .. include:: ../../reference/parameters/imfile-tag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-facility`
     - .. include:: ../../reference/parameters/imfile-facility.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-severity`
     - .. include:: ../../reference/parameters/imfile-severity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-persiststateinterval`
     - .. include:: ../../reference/parameters/imfile-persiststateinterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-startmsg-regex`
     - .. include:: ../../reference/parameters/imfile-startmsg-regex.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-endmsg-regex`
     - .. include:: ../../reference/parameters/imfile-endmsg-regex.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-readtimeout`
     - .. include:: ../../reference/parameters/imfile-readtimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-readmode`
     - .. include:: ../../reference/parameters/imfile-readmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-escapelf`
     - .. include:: ../../reference/parameters/imfile-escapelf.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-escapelf-replacement`
     - .. include:: ../../reference/parameters/imfile-escapelf-replacement.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-maxlinesatonce`
     - .. include:: ../../reference/parameters/imfile-maxlinesatonce.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-maxsubmitatonce`
     - .. include:: ../../reference/parameters/imfile-maxsubmitatonce.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-deletestateonfiledelete`
     - .. include:: ../../reference/parameters/imfile-deletestateonfiledelete.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-deletestateonfilemove`
     - .. include:: ../../reference/parameters/imfile-deletestateonfilemove.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-ruleset`
     - .. include:: ../../reference/parameters/imfile-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-addmetadata`
     - .. include:: ../../reference/parameters/imfile-addmetadata.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-addceetag`
     - .. include:: ../../reference/parameters/imfile-addceetag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-reopenontruncate`
     - .. include:: ../../reference/parameters/imfile-reopenontruncate.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-maxlinesperminute`
     - .. include:: ../../reference/parameters/imfile-maxlinesperminute.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-maxbytesperminute`
     - .. include:: ../../reference/parameters/imfile-maxbytesperminute.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-trimlineoverbytes`
     - .. include:: ../../reference/parameters/imfile-trimlineoverbytes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-freshstarttail`
     - .. include:: ../../reference/parameters/imfile-freshstarttail.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-discardtruncatedmsg`
     - .. include:: ../../reference/parameters/imfile-discardtruncatedmsg.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-msgdiscardingerror`
     - .. include:: ../../reference/parameters/imfile-msgdiscardingerror.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-needparse`
     - .. include:: ../../reference/parameters/imfile-needparse.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-persiststateaftersubmission`
     - .. include:: ../../reference/parameters/imfile-persiststateaftersubmission.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-ignoreolderthan`
     - .. include:: ../../reference/parameters/imfile-ignoreolderthan.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-statefile`
     - .. include:: ../../reference/parameters/imfile-statefile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. _Metadata:

Metadata
========
The imfile module supports message metadata. It supports the following
data items:

- filename

  Name of the file where the message originated from. This is most
  useful when using wildcards inside file monitors, because it then
  is the only way to know which file the message originated from.
  The value can be accessed using the %$!metadata!filename% property.
  **Note**: For symlink-ed files this does **not** contain name of the
  actual file (source of the data) but name of the symlink (file which
  matched configured input).

- fileoffset

  Offset of the file in bytes at the time the message was read. The
  offset reported is from the **start** of the line.
  This information can be useful when recreating multi-line files
  that may have been accessed or transmitted non-sequentially.
  The value can be accessed using the %$!metadata!fileoffset% property.

Metadata is only present if enabled. By default it is enabled for
input() statements that contain wildcards. For all others, it is
disabled by default. It can explicitly be turned on or off via the
*addMetadata* input() parameter, which always overrides the default.


.. _State-Files:

State Files
===========
Rsyslog must keep track of which parts of the monitored file
are already processed. This is done in so-called "state files" that
are created in the rsyslog working directory and are read on startup to
resume monitoring after a shutdown. The location of the rsyslog
working directory is configurable via the ``global(workDirectory)``
|FmtAdvancedName| format parameter.

**Note**: The ``PersistStateInterval`` parameter must be set, otherwise state
files will NOT be created.

Rsyslog automatically generates state file names. These state file
names will begin with the string ``imfile-state:`` and be followed
by some suffix rsyslog generates.

There is intentionally no more precise description of when state file
naming, as this is an implementation detail and may change as needed.

Note that it is possible to set a fixed state file name via the
deprecated :ref:`stateFile <param-imfile-statefile>` parameter. It is suggested to avoid this, as
the user must take care of name clashes. Most importantly, if
"stateFile" is set for file monitors with wildcards, the **same**
state file is used for all occurrences of these files. In short,
this will usually not work and cause confusion. Upon startup,
rsyslog tries to detect these cases and emit warning messages.
However, the detection simply checks for the presence of "*"
and as such it will not cover more complex cases.

Note that when the ``global(workDirectory)`` |FmtAdvancedName| format
parameter is points to a non-writable location, the state file
**will not be generated**. In those cases, the file content will always
be completely re-sent by imfile, because the module does not know that it
already processed parts of that file. if The parameter is not set to all, it
defaults to the file system root, which may or may not be writable by
the rsyslog process.


.. _WildCards:

WildCards
=========

As of rsyslog version 8.25.0 and later, wildcards are fully supported in both directory names and filenames. This allows for flexible file monitoring configurations.

Examples of supported wildcard usage:

*   `/var/log/*.log` (all .log files in /var/log)
*   `/var/log/*/*.log` (all .log files in immediate subdirectories of /var/log)
*   `/var/log/app*/*/*.log` (all .log files in subdirectories of directories starting with "app" in /var/log)

All matching files in all matching subfolders will be monitored.

.. note::
   Using complex wildcard patterns, especially those that match many directories and files, may have an impact on performance. It's advisable to use the most specific patterns possible.

Historically, versions prior to 8.25.0 had limitations, supporting wildcards only in the filename part and not in directory paths.




Caveats/Known Bugs
==================

* symlink may not always be properly processed

Configuration Examples
======================

The following sample monitors two files. If you need just one, remove
the second one. If you need more, add them according to the sample ;).
This code must be placed in /etc/rsyslog.conf (or wherever your distro
puts rsyslog's config files). Note that only commands actually needed
need to be specified. The second file uses less commands and uses
defaults instead.

.. code-block:: none

  module(load="imfile" PollingInterval="10") #needs to be done just once

  # File 1
  input(type="imfile"
        File="/path/to/file1"
        Tag="tag1"
        Severity="error"
        Facility="local7")

  # File 2
  input(type="imfile"
        File="/path/to/file2"
        Tag="tag2")

  # ... and so on ... #


.. toctree::
   :hidden:

   ../../reference/parameters/imfile-addceetag
   ../../reference/parameters/imfile-addmetadata
   ../../reference/parameters/imfile-deletestateonfiledelete
   ../../reference/parameters/imfile-deletestateonfilemove
   ../../reference/parameters/imfile-discardtruncatedmsg
   ../../reference/parameters/imfile-endmsg-regex
   ../../reference/parameters/imfile-escapelf
   ../../reference/parameters/imfile-escapelf-replacement
   ../../reference/parameters/imfile-facility
   ../../reference/parameters/imfile-file
   ../../reference/parameters/imfile-freshstarttail
   ../../reference/parameters/imfile-ignoreolderthan
   ../../reference/parameters/imfile-maxbytesperminute
   ../../reference/parameters/imfile-inotifyfallbackinterval
   ../../reference/parameters/imfile-maxinotifywatches
   ../../reference/parameters/imfile-maxlinesatonce
   ../../reference/parameters/imfile-maxlinesperminute
   ../../reference/parameters/imfile-maxsubmitatonce
   ../../reference/parameters/imfile-mode
   ../../reference/parameters/imfile-msgdiscardingerror
   ../../reference/parameters/imfile-needparse
   ../../reference/parameters/imfile-persiststateaftersubmission
   ../../reference/parameters/imfile-persiststateinterval
   ../../reference/parameters/imfile-pollinginterval
   ../../reference/parameters/imfile-readmode
   ../../reference/parameters/imfile-readtimeout
   ../../reference/parameters/imfile-reopenontruncate
   ../../reference/parameters/imfile-ruleset
   ../../reference/parameters/imfile-severity
   ../../reference/parameters/imfile-sortfiles
   ../../reference/parameters/imfile-startmsg-regex
   ../../reference/parameters/imfile-statefile
   ../../reference/parameters/imfile-statefile-directory
   ../../reference/parameters/imfile-tag
   ../../reference/parameters/imfile-timeoutgranularity
   ../../reference/parameters/imfile-trimlineoverbytes
