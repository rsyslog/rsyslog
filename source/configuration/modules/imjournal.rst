***************************************
imjournal: Systemd Journal Input Module
***************************************

===========================  ===========================================================================
**Module Name:**             **imjournal**
**Author:**                  Milan Bartos <mbartos@redhat.com> (This module is **not** project-supported)
**Available since:**         7.3.11
===========================  ===========================================================================


Purpose
=======

Provides the ability to import structured log messages from systemd
journal to syslog.

Note that this module reads the journal database, what is considered a
relativly performance-intense operation. As such, the performance of a
configuration utilizing this module may be notably slower than when
using `imuxsock <imuxsock.html>`_. The journal provides imuxsock with a
copy of all "classical" syslog messages, however, it does not provide
structured data. Only if that structured data is needed, imjournal must be used.
Otherwise, imjournal may simply be replaced by imuxsock, and we highly
suggest doing so.

We suggest to check out our short presentation on `rsyslog journal
integration <http://youtu.be/GTS7EuSdFKE>`_ to learn more details of
anticipated use cases.

**Warning:** Some versions of systemd journal have problems with
database corruption, which leads to the journal to return the same data
endlessly in a tight loop. This results in massive message duplication
inside rsyslog probably resulting in a denial-of-service when the system
ressouces get exhausted. This can be somewhat mitigated by using proper
rate-limiters, but even then there are spikes of old data which are
endlessly repeated. By default, ratelimiting is activated and permits to
process 20,000 messages within 10 minutes, what should be well enough
for most use cases. If insufficient, use the parameters described below
to adjust the permitted volume. **It is strongly recommended to use this
plugin only if there is hard need to do so.**


Notable Features
================

- :ref:`imjournal-statistic-counter`


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Module Parameters
=================


PersistStateInterval
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "10", "no", "``$imjournalPersistStateInterval``"

This is a global setting. It specifies how often should the journal
state be persisted. The persists happens after each *number-of-messages*.
This option is useful for rsyslog to start reading from the last journal
message it read.


StateFile
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$imjournalStateFile``"

This is a global setting. It specifies where the state file for
persisting journal state is located. If a full path name is given
(starting with "/"), that path is used. Otherwise the given name
is created inside the working directory.


Ratelimit.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "600", "no", "``$imjournalRatelimitInterval``"

Specifies the interval in seconds onto which rate-limiting is to be
applied. If more than ratelimit.burst messages are read during that
interval, further messages up to the end of the interval are
discarded. The number of messages discarded is emitted at the end of
the interval (if there were any discards).
Setting this to value zero turns off ratelimiting. Note that it is
**not recommended to turn of ratelimiting**, except that you know for
sure journal database entries will never be corrupted. Without
ratelimiting, a corrupted systemd journal database may cause a kind
of denial of service (we are stressing this point as multiple users
have reported us such problems with the journal database -
information current as of June 2013).


Ratelimit.Burst
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "20000", "no", "``$imjournalRatelimitBurst``"

Specifies the maximum number of messages that can be emitted within
the ratelimit.interval interval. For futher information, see
description there.


IgnorePreviousMessages
^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$ImjournalIgnorePreviousMessages``"

This option specifies whether imjournal should ignore messages
currently in journal and read only new messages. This option is only
used when there is no StateFile to avoid message loss.


DefaultSeverity
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "severity", "5", "no", "``$ImjournalDefaultSeverity``"

Some messages comming from journald don't have the SYSLOG_PRIORITY
field. These are typically the messages logged through journald's
native API. This option specifies the default severity for these
messages. Can be given either as a name or a number. Defaults to 'notice'.


DefaultFacility
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "facility", "LOG_USER", "no", "``$ImjournalDefaultFacility``"

Some messages comming from journald don't have the SYSLOG_FACILITY
field. These are typically the messages logged through journald's
native API. This option specifies the default facility for these
messages. Can be given either as a name or a number. Defaults to 'user'.


UsePidFromSystem
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "0", "no", "none"

Retrieves the trusted systemd parameter, _PID, instead of the user
systemd parameter, SYSLOG_PID, which is the default.
This option override the "usepid" option.
This is now deprecated. It is better to use usepid="syslog" instead.


UsePid
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "both", "no", "none"

Sets the PID source from journal.

*syslog*
   *imjournal* retrieves SYSLOG_PID from journal as PID number.

*system*
   *imjournal* retrieves _PID from journal as PID number.

*both*
   *imjournal* trying to retrieve SYSLOG_PID first. When it is not
   available, it is also trying to retrive _PID. When none of them is available,
   message is parsed without PID number.


IgnoreNonValidStatefile
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

When a corrupted statefile is read imjournal ignores the statefile and continues
with logging from the beginning of the journal (from its end if IgnorePreviousMessages
is on). After PersistStateInterval or when rsyslog is stopped invalid statefile
is overwritten with a new valid cursor.


.. _imjournal-statistic-counter:

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each listener and for each worker thread. The listener statistic is named "imjournal".

The following properties are maintained for each listener:

-  **submitted** - total number of messages submitted for processing since startup


Caveats/Known Bugs:
===================

- As stated above, a corrupted systemd journal database can cause major
  problems, depending on what the corruption results in. This is beyond
  the control of the rsyslog team.

- imjournal does not check if messages received actually originated
  from rsyslog itself (via omjournal or other means). Depending on
  configuration, this can also lead to a loop. With imuxsock, this
  problem does not exist.


Build Requirements:
===================

Development headers for systemd, version >= 197.


Example
=======

The following example shows pulling structured imjournal messages and
saving them into /var/log/ceelog.

.. code-block:: none

  module(load="imjournal" PersistStateInterval="100"
         StateFile="/path/to/file") #load imjournal module
  module(load="mmjsonparse") #load mmjsonparse module for structured logs

  template(name="CEETemplate" type="string" string="%TIMESTAMP% %HOSTNAME% %syslogtag% @cee: %$!all-json%\n" ) #template for messages

  action(type="mmjsonparse")
  action(type="omfile" file="/var/log/ceelog" template="CEETemplate")


