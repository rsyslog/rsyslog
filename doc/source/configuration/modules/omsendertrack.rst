.. index:: ! omsendertrack

.. _omsendertrack:

omsendertrack: Sender Tracking Output Module
=============================================

.. rst-class:: AdisconInfo

:Module Name: **omsendertrack**
:Author: `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
:Available since: 8.2506.0 (Proof-of-Concept)

**Status:** Proof-of-concept implementation. This module is currently in an
experimental stage. Further details and discussion regarding its development and
progress can be found in `issue #5599 <https://github.com/rsyslog/rsyslog/issues/5599>`_.

Purpose
-------

The ``omsendertrack`` output module is designed to collect and maintain real-time
statistics about message senders across all configured rsyslog inputs. Its
primary goal is to provide a flexible and persistent mechanism for tracking
message flow from various sources.

Key uses for ``omsendertrack`` include:

* **Identifying Top Senders:** Quickly pinpoint which hosts or applications
    are generating the most log traffic.
* **Monitoring Sender Behavior:** Detect changes in message rates or patterns
    from specific senders, which can indicate issues or unusual activity.
* **Understanding Message Distribution:** Gain insights into the overall
    distribution of messages within your logging infrastructure.
* **Persistency:** Message counts and last event times persist across rsyslog
    daemon restarts, ensuring continuous tracking.

The module achieves this by periodically writing these statistics to a JSON
:ref:`statefile <omsendertrack_statefile>`.

Functionality
-------------

The ``omsendertrack`` module operates through several key stages and mechanisms
to ensure accurate and persistent sender tracking.

Initialization
^^^^^^^^^^^^^^

Upon rsyslog startup, the ``omsendertrack`` module attempts to load its
previously saved state from the configured :ref:`statefile
<omsendertrack_statefile>`. This data, which includes sender identifiers,
message counts, and last event times, is loaded into an in-memory hash table.
This ensures that message statistics are restored and tracking continues
seamlessly across daemon restarts. A background task is then spawned to handle
periodic state persistence.

OnAction Call (Message Processing)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When a message is routed to an ``omsendertrack`` action:

1.  **Sender Identification:** The module uses the configured :ref:`senderid
    <omsendertrack_senderid>` template to derive a unique identifier for the
    message sender.
2.  **Statistic Update:** It then updates or inserts an entry for this sender in
    its internal hash table.
3.  **Timestamp Recording:** The `last-event-time` for the sender is updated
    with the current UTC timestamp of the received message.
4.  **Message Counting:** The `message-count` for that sender is incremented.
5.  **Average Rate (Optional):** If configured, the `avg-message-count`
    (average message rate) may also be recalculated.

HUP Signal Handling
^^^^^^^^^^^^^^^^^^^

When rsyslog receives a HUP signal (typically used for configuration reloads),
the ``omsendertrack`` module is designed to check for the existence of a
:ref:`cmdfile <omsendertrack_cmdfile>`. If a `cmdfile` is specified and found,
it would be read and its commands processed. After processing, the `cmdfile`
would be deleted to prevent re-execution on subsequent HUP signals.

**Note:** Command file support is currently **not implemented** in this
proof-of-concept version of the module.

Background Task
^^^^^^^^^^^^^^^

A dedicated background task is responsible for persisting the module's current
state to the configured :ref:`statefile <omsendertrack_statefile>`. This task
wakes up at the `interval` specified in the configuration. It performs atomic
writes to the `statefile` to prevent data corruption, even if rsyslog
unexpectedly terminates during a write operation.

Shutdown
^^^^^^^^

During rsyslog shutdown, the ``omsendertrack`` module ensures that the most
current sender statistics are saved to the :ref:`statefile
<omsendertrack_statefile>`. This critical step guarantees data persistence and
allows for an accurate resumption of tracking when rsyslog restarts.

Configuration
-------------

The ``omsendertrack`` module supports the following action parameters.

.. note::

   Parameter names are case-insensitive.

Action Parameters
-----------------

senderid
^^^^^^^^

.. _omsendertrack_senderid:

.. csv-table::
   :header: "Type", "Default", "Mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "RSYSLOG_FileFormat", "no", "none"

This parameter defines the **template used to determine the sender's unique
identifier**. The value produced by this template will be used as the key for
tracking individual senders within the module's internal statistics.

For instance:

* A simple template like ``"%hostname%"`` will track each unique host that
    submits messages to rsyslog.
* Using ``"%fromhost-ip%"`` will track senders based on their IP address.
* A more granular template such as ``"%hostname%-%app-name%"`` can
    differentiate between applications on the same host.

**Note:** The processing of this template for every incoming message can impact
overall throughput, especially if complex templates are used. Choose your
template wisely based on your tracking needs and performance considerations.

.. important::

   The current Proof-of-Concept implementation of the ``omsendertrack`` module
   might still refer to this parameter as ``template`` instead of ``senderid``.
   Please use ``template`` if ``senderid`` is not recognized by your rsyslog
   version, and be aware that this will be harmonized in future releases.

interval
^^^^^^^^

.. _omsendertrack_interval:

.. csv-table::
   :header: "Type", "Default", "Mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "60", "no", "none"

This parameter defines the **interval in seconds** after which the module
writes the current sender statistics to the configured :ref:`statefile
<omsendertrack_statefile>`.

A smaller `interval` value results in more frequent updates to the state file,
reducing potential data loss in case of an unexpected system crash, but it also
increases disk I/O. A larger `interval` reduces I/O but means less up-to-date
statistics on disk.

statefile
^^^^^^^^^

.. _omsendertrack_statefile:

.. csv-table::
   :header: "Type", "Default", "Mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "yes", "none"

This mandatory parameter specifies the **absolute path to the JSON file** where
sender information will be stored. The module updates this file periodically
based on the :ref:`interval <omsendertrack_interval>` and also upon rsyslog
shutdown to preserve the latest statistics.

**Important:** Ensure that the rsyslog user has appropriate write permissions
to the directory where this `statefile` is located. Failure to do so will
prevent the module from saving its state.

cmdfile
^^^^^^^

.. _omsendertrack_cmdfile:

.. csv-table::
   :header: "Type", "Default", "Mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

This optional parameter allows you to specify the **absolute path to a command
file**. This file *is designed to be processed when rsyslog receives a HUP
signal* (e.g., via `systemctl reload rsyslog`).

**Note:** Command file support is currently **not implemented** in this
proof-of-concept version of the module. When implemented, this feature is
intended to allow dynamic control over the module's behavior, such as resetting
statistics for specific senders, without requiring an rsyslog restart.

Statistic Counter
-----------------

The ``omsendertrack`` module is designed to maintain a set of statistics for
each unique sender identifier it tracks. These statistics are intended to be
periodically serialized and written to the configured :ref:`statefile
<omsendertrack_statefile>` in JSON format.

**Important:** This module **does not offer statistics counters in the typical
sense** that are consumable by other rsyslog modules like `impstats`. The
collected data is primarily intended for direct consumption from the generated
state file.

**Note:** There are currently **no statistics counters available** in this
proof-of-concept version of the module.

The JSON structure for each sender entry is envisioned to look like this:

.. code-block:: json

   {
     "senderid": "value_from_template",
     "last-event-time": "YYYY-MM-DDTHH:MM:SS.sssZ",
     "message-count": "N_VALUE",
     "avg-message-count": "M_POINT_M_VALUE"
   }

Where:

* ``senderid``: The unique identifier for the sender, as determined by the
    :ref:`senderid <omsendertrack_senderid>` template.
* ``last-event-time``: A UTC timestamp (ISO 8601 format) indicating when the
    last message from this sender was received.
* ``message-count``: The total number of messages received from this sender
    since tracking began (or since the last reset).
* ``avg-message-count``: (Optional) The average message rate from this sender
    since tracking began, calculated over the total elapsed time. This field's
    presence depends on future module configuration and implementation details.

Usage within Rsyslog Configuration
----------------------------------

The ``omsendertrack`` module functions as an output module (OM), meaning you
integrate its action where you want sender statistics to be collected within
your rsyslog configuration. Each instance of ``omsendertrack`` counts its
senders independently.

**Queue Considerations:**

It's technically possible to place the ``omsendertrack`` action within a
dedicated ruleset that has a queue, or to add a queue directly to the action
itself. However, ``omsendertrack`` processing is **extremely fast**, with the
overhead of a queue often being multiple times greater than the actual call to
the module. For this reason, adding a queue generally **does not make sense**
for ``omsendertrack`` and is **not recommended** as it would introduce
unnecessary complexity and potential latency without significant benefit.

For optimal performance, always consider calling ``omsendertrack`` actions
synchronously. This can be done within an existing ruleset, or by a synchronous
``call`` statement to a dedicated ruleset that has **no queue**.

Best Practices
--------------

To ensure efficient and correct operation of the ``omsendertrack`` module,
adhere to the following best practices:

* **Prioritize Synchronous Calls:** Always call ``omsendertrack`` actions
    synchronously. The module is highly optimized for quick processing,
    and asynchronous calls with queues are generally unnecessary and can
    introduce overhead without benefit.
* **Avoid Queues on Dedicated Rulesets:** If you use a dedicated ruleset to
    house the ``omsendertrack`` action (as shown in Example 2), ensure that
    this specific ruleset **does not have a queue configured**. The module's
    fast execution makes queues redundant here.
* **Efficient Sender Identification:** Choose your :ref:`senderid <omsendertrack_senderid>` template carefully. Simpler templates (e.g., ``"%hostname%"``, ``"%fromhost-ip%"``) result in better performance, as template processing occurs for every message.
* **Appropriate `interval` for State File Writes:** Balance your need for
    up-to-date statistics against disk I/O. A very small `interval` can lead
    to increased disk writes, while a larger one might mean slightly older data
    on disk in case of an unexpected shutdown.
* **Ensure State File Write Permissions:** Verify that the rsyslog user has
    proper write permissions to the directory specified in the :ref:`statefile
    <omsendertrack_statefile>` parameter. Without this, statistics cannot be
    persisted.
* **Dedicated Ruleset for Unified Stats:** Use a dedicated ruleset that is
    called from multiple input-bound rulesets (Example 2) **only when** you
    need to collect statistics from those diverse inputs into a **single,
    unified sender statistics file**.
* **Multiple Instances for Separate Stats:** Deploy multiple ``omsendertrack``
    action instances (Example 3) **only when** you explicitly desire to generate
    **separate sender statistics files** based on different filtering criteria
    or input sources. Do not create multiple instances if a single, aggregated
    statistic file is your goal.

Examples
--------

Let's look at some examples of how to configure the ``omsendertrack`` module.

Example 1: Basic Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This is the simplest way to use ``omsendertrack``. It loads the module and
configures it to track senders based on their hostname, updating statistics
every 60 seconds and storing them in a state file. This approach is suitable
when all messages you wish to track are processed within a single ruleset or
when the overall volume is low.

.. code-block:: rsyslog

   module(load="omsendertrack")
   action(type="omsendertrack"
          senderid="%hostname%"
          interval="60"
          statefile="/var/lib/rsyslog/senderstats.json")

Example 2: Usage with Dedicated Ruleset
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A dedicated ruleset for ``omsendertrack`` is suggested **specifically when** you
need to count senders where the incoming messages are bound to **different
rulesets**, **and** you want all those messages to contribute to a **single,
unified sender statistics file**.

This example shows how to set up ``omsendertrack`` within a dedicated ruleset,
which is then called synchronously from multiple input-bound rulesets. This
allows you to centralize sender tracking while maintaining separate message
processing flows for other actions.

.. code-block:: rsyslog

   # Define the template for senderid in omsendertrack
   template(name="id-template" type="list") {
       property(name="hostname")
   }

   # Ruleset omsendertrack-ruleset: Must only contain the omsendertrack action
   # This ruleset should NOT have a queue.
   ruleset(name="omsendertrack-ruleset") {
       action(
           type="omsendertrack"
           senderid="id-template"
           interval="60"
           statefile="/var/lib/rsyslog/senderstats.json"
           cmdfile="/var/lib/rsyslog/sendercommands.txt"
       )
   }

   # Ruleset a: Calls omsendertrack-ruleset synchronously, then forwards messages
   ruleset(name="a"
       queue.type="LinkedList"
       queue.spoolDirectory="/var/lib/rsyslog/queue_a"
       queue.fileName="q_a"
       queue.maxDiskSpace="1g"
       queue.saveOnShutdown="on"
       queue.discardSeverity="8"
       queue.discardMark="1"
   ) {
       call omsendertrack-ruleset
       action(
           type="omfwd"
           target="192.0.2.1"
           port="10514"
           protocol="udp"
       )
       action(
           type="omfwd"
           target="192.0.2.2"
           port="10514"
           protocol="tcp"
       )
   }

   # Ruleset b: Calls omsendertrack-ruleset synchronously, then forwards messages
   ruleset(name="b"
       queue.type="LinkedList"
       queue.spoolDirectory="/var/lib/rsyslog/queue_b"
       queue.fileName="q_b"
       queue.maxDiskSpace="1g"
       queue.saveOnShutdown="on"
       queue.discardSeverity="8"
       queue.discardMark="1"
   ) {
       call omsendertrack-ruleset
       action(
           type="omfwd"
           target="192.0.2.3"
           port="514"
           protocol="udp"
       )
       action(
           type="omfwd"
           target="192.0.2.4"
           port="514"
           protocol="tcp"
       )
   }

   # Input for ruleset a (example: UDP input)
   input(type="imudp" port="5140" ruleset="a")

   # Input for ruleset b (example: TCP input)
   input(type="imtcp" port="5141" ruleset="b")

   # Default ruleset (if messages don't match other inputs)
   # This is here for completeness, you can remove or modify it as needed.
   ruleset(name="RSYSLOG_DefaultRuleset") {
       stop
   }

Example 3: Multiple Instances for Separate Statistics
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

It is possible to use multiple instances of ``omsendertrack`` if it is desired
to create **separate sender statistics files** based on different criteria.
For example, you might want to track UDP senders and TCP senders in distinct
state files.

.. code-block:: rsyslog

   # Track UDP senders in a separate state file
   ruleset(name="udp-sender-tracking") {
       action(
           type="omsendertrack"
           senderid="%fromhost-ip%"
           interval="300"
           statefile="/var/lib/rsyslog/udp_sender_stats.json"
       )
       # Add other actions for UDP messages here (e.g., forwarding, writing to file)
   }

   # Track TCP senders in another state file
   ruleset(name="tcp-sender-tracking") {
       action(
           type="omsendertrack"
           senderid="%fromhost-ip%"
           interval="300"
           statefile="/var/lib/rsyslog/tcp_sender_stats.json"
       )
       # Add other actions for TCP messages here
   }

   # Bind inputs to the respective sender tracking rulesets
   input(type="imudp" port="514" ruleset="udp-sender-tracking")
   input(type="imtcp" port="514" ruleset="tcp-sender-tracking")

   # Further processing for all messages (e.g., default ruleset)
   ruleset(name="RSYSLOG_DefaultRuleset") {
       stop
   }
