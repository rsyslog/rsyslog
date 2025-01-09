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

   Parameter names are case-insensitive.


Module Parameters
-----------------

Mode
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "inotify", "no", "none"

.. versionadded:: 8.1.5

This specifies if imfile is shall run in inotify ("inotify") or polling
("polling") mode. Traditionally, imfile used polling mode, which is
much more resource-intense (and slower) than inotify mode. It is
suggested that users turn on "polling" mode only if they experience
strange problems in inotify mode. In theory, there should never be a
reason to enable "polling" mode and later versions will most probably
remove it.

Note: if a legacy "$ModLoad" statement is used, the default is *polling*.
This default was kept to prevent problems with old configurations. It
might change in the future.

.. versionadded:: 8.32.0

On Solaris, the FEN API is used instead of INOTIFY. You can set the mode
to fen or inotify (which is automatically mapped to fen on Solaris OS).
Please note that the FEN is limited compared to INOTIFY. Deep wildcard
matches may not work because of the API limits for now.


readTimeout
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

*Default: 0 (no timeout)*

.. versionadded:: 8.23.0

This sets the default value for input *timeout* parameters. See there
for exact meaning. Parameter value is the number of seconds.


timeoutGranularity
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1", "no", "none"

.. versionadded:: 8.23.0

This sets the interval in which multi-line-read timeouts are checked.
The interval is specified in seconds. Note that
this establishes a lower limit on the length of the timeout. For example, if
a timeoutGranularity of 60 seconds is selected and a readTimeout value of 10 seconds
is used, the timeout is nevertheless only checked every 60 seconds (if there is
no other activity in imfile). This means that the readTimeout is also only
checked every 60 seconds, which in turn means a timeout can occur only after 60
seconds.

Note that timeGranularity has some performance implication. The more frequently
timeout processing is triggered, the more processing time is needed. This
effect should be negligible, except if a very large number of files is being
monitored.


sortFiles
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.32.0

If this parameter is set to on, the files will be processed in sorted order, else
not. However, due to the inherent asynchronicity of the whole operations involved
in tracking files, it is not possible to guarantee this sorted order, as it also
depends on operation mode and OS timing.


PollingInterval
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "10", "no", "none"

This setting specifies how often files are to be
polled for new data. For obvious reasons, it has effect only if
imfile is running in polling mode.
The time specified is in seconds. During each
polling interval, all files are processed in a round-robin fashion.

A short poll interval provides more rapid message forwarding, but
requires more system resources. While it is possible, we strongly
recommend not to set the polling interval to 0 seconds. That will
make rsyslogd become a CPU hog, taking up considerable resources. It
is supported, however, for the few very unusual situations where this
level may be needed. Even if you need quick response, 1 seconds
should be well enough. Please note that imfile keeps reading files as
long as there is any data in them. So a "polling sleep" will only
happen when nothing is left to be processed.

**We recommend to use inotify mode.**


statefile.directory
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "global(WorkDirectory) value", "no", "none"

.. versionadded:: 8.1905.0

This parameter permits to specify a dedicated directory for the storage of
imfile state files. An absolute path name should be specified (e.g.
`/var/rsyslog/imfilestate`). This permits to keep imfile state files separate
from other rsyslog work items.

If not specified the global `workDirectory` setting is used.

**Important: The directory must exist before rsyslog is started.** Also,
rsyslog needs write permissions to work correctly. Keep in mind that this
also might require SELinux definitions (or similar for other enhanced security
systems).


Input Parameters
----------------

File
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "yes", "``$InputFileName``"

The file being monitored. So far, this must be an absolute name (no
macros or templates). Note that wildcards are supported at the file
name level (see **WildCards** below for more details).


Tag
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "yes", "``$InputFileTag``"

The tag to be assigned to messages read from this file. If you would like to
see the colon after the tag, you need to include it when you assign a tag
value, like so: ``tag="myTagValue:"``.


Facility
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer or string (preferred)", "local0", "no", "``$InputFileFacility``"

The syslog facility to be assigned to messages read from this file. Can be
specified in textual form (e.g. ``local0``, ``local1``, ...) or as numbers (e.g.
16 for ``local0``). Textual form is suggested. Default  is ``local0``.

.. seealso::

   https://en.wikipedia.org/wiki/Syslog


Severity
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer or string (preferred)", "notice", "no", "``$InputFileSeverity``"

The syslog severity to be assigned to lines read. Can be specified
in textual   form (e.g. ``info``, ``warning``, ...) or as numbers (e.g. 6
for ``info``). Textual form is suggested. Default is ``notice``.

.. seealso::

   https://en.wikipedia.org/wiki/Syslog


PersistStateInterval
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$InputFilePersistStateInterval``"

Specifies how often the state file shall be written when processing
the input file. The **default** value is 0, which means a new state
file is at least being written when the monitored files is being closed (end of
rsyslogd execution). Any other value n means that the state file is
written at least every time n file lines have been processed. This setting can
be used to guard against message duplication due to fatal errors
(like power fail). Note that this setting affects imfile performance,
especially when set to a low value. Frequently writing the state file
is very time consuming.

Note further that rsyslog may write state files
more frequently. This happens if rsyslog has some reason to do so.
There is intentionally no more precise description of when state files
are being written, as this is an implementation detail and may change
as needed.

**Note: If this parameter is not set, state files are not created.**


startmsg.regex
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

.. versionadded:: 8.10.0

This permits the processing of multi-line messages. When set, a
messages is terminated when the next one begins, and
``startmsg.regex`` contains the regex that identifies the start
of a message. As this parameter is using regular expressions, it
is more flexible than ``readMode`` but at the cost of lower
performance.
Note that ``readMode`` and ``startmsg.regex`` and ``endmsg.regex`` cannot all be
defined for the same input.


endmsg.regex
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

.. versionadded:: 8.38.0

This permits the processing of multi-line messages. When set, a message is
terminated when ``endmsg.regex`` matches the line that
identifies the end of a message. As this parameter is using regular
expressions, it is more flexible than ``readMode`` but at the cost of lower
performance.
Note that ``readMode`` and ``startmsg.regex`` and ``endmsg.regex`` cannot all be
defined for the same input.
The primary use case for this is multiline container log files which look like
this:

.. code-block:: none

    date stdout P start of message
    date stdout P  middle of message
    date stdout F  end of message

The `F` means this is the line which contains the final part of the message.
The fully assembled message should be `start of message middle of message end of
message`.  `endmsg.regex="^[^ ]+ stdout F "` will match.

readTimeout
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

.. versionadded:: 8.23.0

This can be used with *startmsg.regex* (but not *readMode*). If specified,
partial multi-line reads are timed out after the specified timeout interval.
That means the current message fragment is being processed and the next
message fragment arriving is treated as a completely new message. The
typical use case for this parameter is a file that is infrequently being
written. In such cases, the next message arrives relatively late, maybe hours
later. Specifying a readTimeout will ensure that those "last messages" are
emitted in a timely manner. In this use case, the "partial" messages being
processed are actually full messages, so everything is fully correct.

To guard against accidental too-early emission of a (partial) message, the
timeout should be sufficiently large (5 to 10 seconds or more recommended).
Specifying a value of zero turns off timeout processing. Also note the
relationship to the *timeoutGranularity* global parameter, which sets the
lower bound of *readTimeout*.

Setting timeout values slightly increases processing time requirements; the
effect should only be visible of a very large number of files is being
monitored.


readMode
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$InputFileReadMode``"

This provides support for processing some standard types of multiline
messages. It is less flexible than ``startmsg.regex`` or ``endmsg.regex`` but
offers higher performance than regex processing. Note that ``readMode`` and
``startmsg.regex`` and ``endmsg.regex`` cannot all be defined for the same
input.

The value can range from 0-2 and determines the multiline
detection method.

0 - (**default**) line based (each line is a new message)

1 - paragraph (There is a blank line between log messages)

2 - indented (new log messages start at the beginning of a line. If a
line starts with a space or tab "\t" it is part of the log message before it)


escapeLF
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "1", "no", "none"

This is only meaningful if multi-line messages are to be processed.
LF characters embedded into syslog messages cause a lot of trouble,
as most tools and even the legacy syslog TCP protocol do not expect
these. If set to "on", this option avoid this trouble by properly
escaping LF characters to the 4-byte sequence "#012". This is
consistent with other rsyslog control character escaping. By default,
escaping is turned on. If you turn it off, make sure you test very
carefully with all associated tools. Please note that if you intend
to use plain TCP syslog with embedded LF characters, you need to
enable octet-counted framing. For more details, see
`Rainer Gerhards' blog posting on imfile LF escaping <https://rainer.gerhards.net/2013/09/imfile-multi-line-messages.html>`_.


escapeLF.replacement
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "depending on use", "no", "none"

.. versionadded:: 8.2001.0

This parameter works in conjunction with `escapeLF`. It is only
honored if `escapeLF="on"`.

It permits to replace the default escape sequence by a different character
sequence. The default historically is inconsistent and depends on which
functionality is used to read the file. It can be either "#012" or "\\n". If
you want to retain that default, do not configure this parameter.

If it is configured, any sequence may be used. For example, to replace an LF
with a simple space, use::

   escapeLF.replacement=" "

It is also possible to configure longer replacements. An example for this is::

   escapeLF.replacement="[LF]"

Finally, it is possible to completely remove the LF. This is done by specifying
an empty replacement sequence::

   escapeLF.replacement=""


MaxLinesAtOnce
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$InputFileMaxLinesAtOnce``"

This is a legacy setting that only is supported in *polling* mode.
In *inotify* mode, it is fixed at 0 and all attempts to configure
a different value will be ignored, but will generate an error
message.

Please note that future versions of imfile may not support this
parameter at all. So it is suggested to not use it.

In *polling* mode, if set to 0, each file will be fully processed and
then processing switches to the next file. If it is set to any other
value, a maximum of [number] lines is processed in sequence for each file,
and then the file is switched. This provides a kind of multiplexing
the load of multiple files and probably leads to a more natural
distribution of events when multiple busy files are monitored. For
*polling* mode, the **default** is 10240.


MaxSubmitAtOnce
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1024", "no", "none"

This is an expert option. It can be used to set the maximum input
batch size that imfile can generate. The **default** is 1024, which
is suitable for a wide range of applications. Be sure to understand
rsyslog message batch processing before you modify this option. If
you do not know what this doc here talks about, this is a good
indication that you should NOT modify the default.


deleteStateOnFileDelete
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

This parameter controls if state files are deleted if their associated
main file is deleted. Usually, this is a good idea, because otherwise
problems would occur if a new file with the same name is created. In
that case, imfile would pick up reading from the last position in
the **deleted** file, which usually is not what you want.

However, there is one situation where not deleting associated state
file makes sense: this is the case if a monitored file is modified
with an editor (like vi or gedit). Most editors write out modifications
by deleting the old file and creating a new now. If the state file
would be deleted in that case, all of the file would be reprocessed,
something that's probably not intended in most case. As a side-note,
it is strongly suggested *not* to modify monitored files with
editors. In any case, in such a situation, it makes sense to
disable state file deletion. That also applies to similar use
cases.

In general, this parameter should only by set if the users
knows exactly why this is required.


Ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$InputFileBindRuleset``"

Binds the listener to a specific :doc:`ruleset <../../concepts/multi_ruleset>`.


addMetadata
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "-1", "no", "none"

**Default: see intro section on Metadata**

This is used to turn on or off the addition of metadata to the
message object.


addCeeTag
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

This is used to turn on or off the addition of the "@cee:" cookie to the
message object.


reopenOnTruncate
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

This is an **experimental** feature that tells rsyslog to reopen input file
when it was truncated (inode unchanged but file size on disk is less than
current offset in memory).


MaxLinesPerMinute
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

Instructs rsyslog to enqueue up to the specified maximum number of lines
as messages per minute. Lines above this value are discarded.

The **default** value is 0, which means that no lines are discarded.


MaxBytesPerMinute
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

Instructs rsyslog to enqueue a maximum number of bytes as messages per
minute. Once MaxBytesPerMinute is reached, subsequent messages are
discarded.

Note that messages are not truncated as a result of MaxBytesPerMinute,
rather the entire message is discarded if part of it would be above the
specified maximum bytes per minute.

The **default** value is 0, which means that no messages are discarded.


trimLineOverBytes
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

This is used to tell rsyslog to truncate the line which length is greater
than specified bytes. If it is positive number, rsyslog truncate the line
at specified bytes. Default value of 'trimLineOverBytes' is 0, means never
truncate line.

This option can be used when ``readMode`` is 0 or 2.


freshStartTail
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

This is used to tell rsyslog to seek to the end/tail of input files
(discard old logs) **at its first start(freshStart)** and process only new
log messages.

When deploy rsyslog to a large number of servers, we may only care about
new log messages generated after the deployment. set **freshstartTail**
to **on** will discard old logs. Otherwise, there may be vast useless
message burst on the remote central log receiver

This parameter only applies to files that are already existing during
rsyslog's initial processing of the file monitors.

.. warning::

   Depending on the number and location of existing files, this initial
   startup processing may take some time as well. If another process
   creates a new file at exactly the time of startup processing and writes
   data to it, rsyslog might detect this file and it's data as preexisting
   and may skip it. This race is inevitable. So when freshStartTail is used,
   some risk of data loss exists. The same holds true if between the last
   shutdown of rsyslog and its restart log file content has been added.
   As such, the rsyslog team advises against activating the freshStartTail
   option.


discardTruncatedMsg
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When messages are too long they are truncated and the following part is
processed as a new message. When this parameter is turned on the
truncated part is not processed but discarded.


msgDiscardingError
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

Upon truncation an error is given. When this parameter is turned off, no
error will be shown upon truncation.


needParse
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.1903.0

By default, read message are sent to output modules without passing through
parsers. This parameter informs rsyslog to use also defined parser module(s).



persistStateAfterSubmission
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.2006.0

This setting makes imfile persist state file information after a batch of
messages has been submitted. It can be activated (switched to "on") in order
to provide enhanced robustness against unclean shutdowns. Depending on the
configuration of the rest of rsyslog (most importantly queues), persisting
the state file after each message submission prevents message loss
when reading files and the system is shutdown in an unclean way (e.g.
loss of power).

Please note that this setting may cause frequent state file writes and
as such may cause some performance degradation.


ignoreOlderThan
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

.. versionadded:: 8.2108.0

Instructs imfile to ignore a discovered file that has not been modified in the
specified number of seconds. Once a file is discovered, the file is no longer
ignored and new data will be read. This option is disabled (set to 0) by default.



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
deprecated ``stateFile`` parameter. It is suggested to avoid this, as
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

**Before Version: 8.25.0**
  Wildcards are only supported in the filename part, not in directory names.

* /var/log/\*.log **works**. *
* /var/log/\*/syslog.log does **not work**. *


**Since Version: 8.25.0**
  Wildcards are supported in filename and paths which means these samples will work:

* /var/log/\*.log **works**. *
* /var/log/\*/syslog.log **works**. *
* /var/log/\*/\*.log **works**. *


  All matching files in all matching subfolders will work.
  Note that this may decrease performance in imfile depending on how
  many directories and files are being watched dynamically.




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


Deprecated parameters
=====================

**Note:** While these parameters are still accepted, they should no longer be
used for newly created configurations.

stateFile
---------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$InputFileStateFile``"

This is the name of this file's state file. This parameter should
usually **not** be used. Check the section on "State Files" above
for more details.


