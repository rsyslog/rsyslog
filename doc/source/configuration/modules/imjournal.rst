***************************************
imjournal: Systemd Journal Input Module
***************************************

===========================  ===========================================================================
**Module Name:**             **imjournal**
**Author:**                  Jiri Vymazal <jvymazal@redhat.com> (This module is **not** project-supported)
**Available since:**         7.3.11
===========================  ===========================================================================


Purpose
=======

Provides the ability to import structured log messages from systemd
journal to syslog.

Note that this module reads the journal database, what is considered a
relatively performance-intense operation. As such, the performance of a
configuration utilizing this module may be notably slower than when
using `imuxsock <imuxsock.html>`_. The journal provides imuxsock with a
copy of all "classical" syslog messages, however, it does not provide
structured data. Only if that structured data is needed, imjournal must be used.
Otherwise, imjournal may simply be replaced by imuxsock, and we highly
suggest doing so.

We suggest to check out our short presentation on `rsyslog journal
integration <http://youtu.be/GTS7EuSdFKE>`_ to learn more details of
anticipated use cases.

**Historical warning:** very old systemd journal releases once suffered
from a database corruption defect that caused the journal API to return
the same records in a tight loop. When that happened, rsyslog received
the repeated entries and could exhaust system resources with duplicate
messages. The systemd project resolved the underlying problem years ago
and we have not seen it on maintained distributions for a long time. This note is kept for context, as the built-in rate-limiter was added to protect against this issue. By default, ratelimiting is
activated and permits the processing of 20,000 messages within 10
minutes, which should be sufficient for most use cases. If insufficient,
use the parameters described below to adjust the permitted volume.


Notable Features
================

- statistics counters


Module Parameters
=================

.. note::
   Parameter names are case-insensitive; CamelCase is recommended for readability.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imjournal-persiststateinterval`
     - .. include:: ../../reference/parameters/imjournal-persiststateinterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-filecreatemode`
     - .. include:: ../../reference/parameters/imjournal-filecreatemode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-statefile`
     - .. include:: ../../reference/parameters/imjournal-statefile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-ratelimit-interval`
     - .. include:: ../../reference/parameters/imjournal-ratelimit-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-ratelimit-burst`
     - .. include:: ../../reference/parameters/imjournal-ratelimit-burst.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-ignorepreviousmessages`
     - .. include:: ../../reference/parameters/imjournal-ignorepreviousmessages.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-defaultseverity`
     - .. include:: ../../reference/parameters/imjournal-defaultseverity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-defaultfacility`
     - .. include:: ../../reference/parameters/imjournal-defaultfacility.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-usepidfromsystem`
     - .. include:: ../../reference/parameters/imjournal-usepidfromsystem.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-usepid`
     - .. include:: ../../reference/parameters/imjournal-usepid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-ignorenonvalidstatefile`
     - .. include:: ../../reference/parameters/imjournal-ignorenonvalidstatefile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-workaroundjournalbug`
     - .. include:: ../../reference/parameters/imjournal-workaroundjournalbug.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-fsync`
     - .. include:: ../../reference/parameters/imjournal-fsync.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-remote`
     - .. include:: ../../reference/parameters/imjournal-remote.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imjournal-defaulttag`
     - .. include:: ../../reference/parameters/imjournal-defaulttag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Input Parameters
================

Parameters specific to the input module.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imjournal-main`
     - .. include:: ../../reference/parameters/imjournal-main.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/imjournal-persiststateinterval
   ../../reference/parameters/imjournal-filecreatemode
   ../../reference/parameters/imjournal-statefile
   ../../reference/parameters/imjournal-ratelimit-interval
   ../../reference/parameters/imjournal-ratelimit-burst
   ../../reference/parameters/imjournal-ignorepreviousmessages
   ../../reference/parameters/imjournal-defaultseverity
   ../../reference/parameters/imjournal-defaultfacility
   ../../reference/parameters/imjournal-usepidfromsystem
   ../../reference/parameters/imjournal-usepid
   ../../reference/parameters/imjournal-ignorenonvalidstatefile
   ../../reference/parameters/imjournal-workaroundjournalbug
   ../../reference/parameters/imjournal-fsync
   ../../reference/parameters/imjournal-remote
   ../../reference/parameters/imjournal-defaulttag
   ../../reference/parameters/imjournal-main



Statistic Counter
=================

.. _imjournal-statistic-counter:

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each listener and for each worker thread. The listener statistic is named "imjournal".

The following properties are maintained for each listener:

-  **read** - total number of message read from journal since startup.

-  **submitted** - total number of messages submitted to main queue after reading from journal for processing
   since startup. All records may not be submitted due to rate-limiting.

-  **discarded** - total number of messages that were read but not submitted to main queue due to rate-limiting.

-  **failed** - total number of failures to read messages from journal.

-  **rotations** - total number of journal file rotations.

-  **recovery_attempts** - total number of recovery attempts by imjournal after unknown errors by closing and
   re-opening journal.

-  **ratelimit_discarded_in_interval** - number of messages discarded due to rate-limiting within configured
   rate-limiting interval.

-  **disk_usage_bytes** - total size of journal obtained from sd_journal_get_usage().

Here is an example output of corresponding imjournal impstat message, which is produced by loading imjournal
with default rate-limit interval and burst and running a docker container with log-driver as journald that
spews lots of logs to stdout:

.. code-block:: none

	Jun 13 15:02:48 app1-1.example.com rsyslogd-pstats: imjournal: origin=imjournal submitted=20000 read=216557
	discarded=196557 failed=0 rotations=6 recovery_attempts=0 ratelimit_discarded_in_interval=196557
	disk_usage_bytes=106610688

Although these counters provide insight into imjournal end message submissions to main queue as well as losses due to
rate-limiting or other problems to extract messages from journal, they don't offer full visibility into journal end
issues. While these counters measure journal rotations and disk usage, they do not offer visibility into message
loss due to journal rate-limiting. sd_journal_* API does not provide any visibility into messages that are
discarded by the journal due to rate-limiting. Journald does emit a syslog message when log messages cannot make
it into the journal due to rate-limiting:

.. code-block::	none

	Jun 13 15:50:32 app1-1.example.com systemd-journal[333]: Suppressed 102 messages from /system.slice/docker.service

Such messages can be processed after they are read through imjournal to get a signal for message loss due to journal
end rate-limiting using a dynamic statistics counter for such log lines with a rule like this:

.. code-block:: none

	dyn_stats(name="journal" resettable="off")
	if $programname == 'journal' and $msg contains 'Suppressed' and $msg contains 'messages from' then {
		set $.inc = dyn_inc("journal", "suppressed_count");
	}

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


Example 1
=========

The following example shows pulling structured imjournal messages and
saving them into /var/log/ceelog.

.. code-block:: none

  module(load="imjournal" PersistStateInterval="100"
         StateFile="/path/to/file") #load imjournal module
  module(load="mmjsonparse") #load mmjsonparse module for structured logs

  template(name="CEETemplate" type="string" string="%TIMESTAMP% %HOSTNAME% %syslogtag% @cee: %$!all-json%\n" ) #template for messages

  action(type="mmjsonparse")
  action(type="omfile" file="/var/log/ceelog" template="CEETemplate")


Example 2
=========

The following example is the same as `Example 1`, but with the input module.

.. code-block:: none

  ruleset(name="imjournal-example" queue.type="direct"){
   action(type="mmjsonparse")
   action(type="omfile" file="/var/log/ceelog" template="CEETemplate")
  }

  input(
   type="imjournal"
   ruleset="imjournal-example"
   main="on"
  )
