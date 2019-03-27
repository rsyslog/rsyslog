**************************************
imtuxedoulog: Tuxedo ULOG input module
**************************************

================  ==============================================================
**Module Name:**  **imtuxedoulog**
**Authors:**      Jean-Philippe Hilaire <jean-philippe.hilaire@pmu.fr> & Philippe Duveau <philippe.duveau@free.fr>
================  ==============================================================


Purpose
=======

This module allows rsyslog to process Tuxedo ULOG files.
Tuxedo create an ULOG file each new log of the day this file is defined

- a prefix configured in the tuxedo configuration

- a suffix based on the date ".MMDDYY"

This module is a copy of the polling mode of imfile but the file name is
computed each polling. The previous one is closed to avoid to much active
file descriptor simultaneously.

Another particularity of ULOG is that the lines contains only the time in
day. So the module use the date in filename and time in log to fill log
timestamp.

Compile
=======

To successfully compile improg module.

    ./configure --enable-imtuxedoulog ...

Configuration Parameters
========================

Action Parameters
-----------------

ulogbase
^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "yes", "path of ULOG file",   

Path of ULOG file as it is defined in Tuxedo Configuration ULOGPFX.
Dot and date is added a end to build full file path

Tag
^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "yes", ,"none"

The tag to be assigned to messages read from this file. If you would like to
see the colon after the tag, you need to include it when you assign a tag
value, like so: ``tag="myTagValue:"``.

Facility
^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "facility\|number", "local0" 

The syslog facility to be assigned to messages read from this file. Can be
specified in textual form (e.g. ``local0``, ``local1``, ...) or as numbers (e.g.
16 for ``local0``). Textual form is suggested.

Severity
^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "severity\|number", "notice"

The syslog severity to be assigned to lines read. Can be specified
in textual   form (e.g. ``info``, ``warning``, ...) or as numbers (e.g. 6
for ``info``). Textual form is suggested.

PersistStateInterval
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$InputFilePersistStateInterval``"

Specifies how often the state file shall be written when processing
the input file. The **default** value is 0, which means a new state
file is only written when the monitored files is being closed (end of
rsyslogd execution). Any other value n means that the state file is
written every time n file lines have been processed. This setting can
be used to guard against message duplication due to fatal errors
(like power fail). Note that this setting affects imfile performance,
especially when set to a low value. Frequently writing the state file
is very time consuming.

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
and then the file is switched. This provides a kind of mutiplexing
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
