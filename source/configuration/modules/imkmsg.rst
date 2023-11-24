**********************************
imkmsg: /dev/kmsg Log Input Module
**********************************

===========================  ===========================================================================
**Module Name:**             **imkmsg**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
                             Milan Bartos <mbartos@redhat.com>
===========================  ===========================================================================


Purpose
=======

Reads messages from the /dev/kmsg structured kernel log and submits them
to the syslog engine.

The printk log buffer contains log records. These records are exported
by /dev/kmsg device as structured data in the following format:
"level,sequnum,timestamp;<message text>\\n"
There could be continuation lines starting with space that contains
key/value pairs.
Log messages are parsed as necessary into rsyslog msg\_t structure.
Continuation lines are parsed as json key/value pairs and added into
rsyslog's message json representation.


Configuration Parameters
========================

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

   "word", "parseKernelTimestamp", "no", "none"

.. versionadded:: 8.2312.0

This parameter configures which timestamps will be used. It is an advanced
setting and most users should probably keep the default mode ("startup").

The linux kernel message buffer contains a timestamp, which reflects the time
the message was created. However, this is not the "normal" time one expects, but
in a so-called monotonic time in seconds since kernel start. For a datacenter
system which runs 24 hours by 7 days a week, kernel time and actual
wall clock time is mostly the same. Problems may occur during daylight
savings time switches.

For desktops and laptops this is not necessarily the case. The reason is, as
it looks, that during low power states (energy save mode, hibernation), kernel
monotonic time **does not advance**. This is also **not** corrected when the
system comes back to normal operations. As such, on systems using low power
states from time to time, kernel time and wallclock time drift apart. We have
been told cases where this is in the magnitude of days. Just think about
desktops which are in hibernate during the night, missing several hours
each day. So this is a real-world problem.

To work around this, we usually do **not** use the kernel timstamp when
we calculate the message time. Instead, we use wallclock time (obtained
from the respective linux timer) of the instant when imkmsg reads the
message from the kernel log. As message creation and imkmsg reading it
is usually in very close time proximity, this approach works very well.

**However**, this is not helpful for e.g. early boot messages. These
were potentially generated some seconds to a minute or two before rsyslog
startup. To provide a proper meaning of time for these events, we use
the kernel timstamp instead of wallclock time during rsyslog startup.
This is most probably correct, because it is extremely unlikely (close
to impossible) that the system entered a low-power state before rsyslog
startup.

**Note well:** When rsyslog is restarted during normal system operations,
existing imkmsg messages are re-read and this is done with the kernel
timestamp. This causes message duplication, but is what imkmsg always
did. It is planned to provide ehance the module to improve this
behaviour. This documentation page here will be updated when changes are
made.

The *parseKernelTimestamp* parameter provides fine-grain control over
the processing of kernel vs. wallclock time. Adjustments should only
be needed rarely and if there is a dedicated use case for it. So use
this parameter only if you have a good reason to do so.

Supported modes are:

* **startup** - This is the **DEFAULT setting**.

  Uses the kernel time stamp during the initial read
  loop of /dev/kmsg, but uses system wallclock time once the initial
  read is completed. This behavior is described in the text above in
  detail.

* **on** - kernel timestamps are always used and wallclock time never

* **off** - kernel timestamps are never used, system wallclock time is
  always used


readMode
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "full-boot", "no", "none"

.. versionadded:: 8.2312.0

This parameter permits to control when imkmsg reads the full kernel.

It provides the following options:

* **full-boot** - (default) read full klog, but only "immediately" after
  boot. "Immediately" is hereby meant in seconds of system uptime
  given in "expectedBootCompleteSeconds"

* **full-always** - read full klog on every rsyslog startup. Most probably
  causes message duplication

* **new-only** - never emit existing kernel log message, read only new ones.

Note that some message loss can happen if rsyslog is stopped in "full-boot" and
"new-only" read mode. The longer rsyslog is inactive, the higher the message
loss probability and potential number of messages lost. For typical restart
scenarios, this should be minimal. On HUP, no message loss occurs as rsyslog
is not actually stopped.


expectedBootCompleteSeconds
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "posisitve integer", "90", "no", "none"

.. versionadded:: 8.2312.0

This parameter works in conjunction with **readMode** and specifies how
many seconds after startup the system should be considered to be
"just booted", which means in **readMode** "full-boot" imkmsg reads and
forwards to rsyslog processing all existing messages.

In any other **readMode** the **expectedBootCompleteSettings** is
ignored.

Caveats/Known Bugs:
===================

This module cannot be used together with imklog module. When using one of
them, make sure the other one is not enabled.

This is Linux specific module and requires /dev/kmsg device with
structured kernel logs.

This module does not support rulesets. All messages are delivered to the
default rulseset.



Examples
========

The following sample pulls messages from the /dev/kmsg log device. All
parameters are left by default, which is usually a good idea. Please
note that loading the plugin is sufficient to activate it. No directive
is needed to start pulling messages.

.. code-block:: none

   module(load="imkmsg")


