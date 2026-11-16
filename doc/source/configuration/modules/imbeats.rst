.. SPDX-License-Identifier: Apache-2.0

******************************
imbeats: Beats v2 input module
******************************

.. meta::
   :description: Receive Elastic Beats and Elastic Agent Logstash output in rsyslog via Lumberjack v2 over TCP or TLS.
   :keywords: rsyslog, imbeats, beats, elastic agent, filebeat, logstash output, lumberjack, input, tcp, tls

.. summary-start

imbeats receives Elastic Beats and Elastic Agent ``output.logstash`` events via
Lumberjack protocol v2 over TCP or TLS, keeps the original JSON payload in
``msg``, maps decoded event fields into the top-level structured tree ``$!``,
and stores transport/protocol metadata under ``$!metadata!imbeats``.

.. summary-end

===========================  ===========================================================================
**Module Name:**             **imbeats**
**Author:**                  **Adiscon and contributors**
**Available since:**         **8.2604.0**
===========================  ===========================================================================


Purpose
========

``imbeats`` accepts Elastic Beats and Elastic Agent traffic that uses the
Logstash-style Lumberjack v2 protocol. Configure Beats or Elastic Agent with
``output.logstash`` and point it at the rsyslog listener. The module reuses
rsyslog's ``netstrm`` transport subsystem, so it can listen via plain TCP or
the configured TLS stream driver.

The first implementation supports:

- Lumberjack v2 only
- ``W`` window frames
- ``J`` JSON event frames
- ``C`` compressed frames
- ``A`` acknowledgements carrying the last sequence number of each window

The first implementation intentionally optimizes the internal event shape for
common Elasticsearch-oriented pipelines:

- ``msg`` keeps the original JSON payload
- decoded Beat event fields are added under top-level ``$!``
- transport and protocol metadata is stored under ``$!metadata!imbeats``
- listener-side size limits reject oversized windows, frames, batches, and
  compressed payload expansion before unbounded allocation
- instance-wide accounting bounds memory held by incomplete sessions, inflate
  scratch space, event copies, and window descriptors
- idle, absolute frame, and absolute window deadlines release sessions that
  stop making progress or retain incomplete protocol state for too long
- listener-side session limits and worker settings bound concurrent sender
  fan-in and provide fairness between active sessions

The metadata object is owned by the module: a ``metadata`` member sent by
the client cannot override ``$!metadata!imbeats``. If a client event carries a
top-level ``metadata`` member that is not a JSON object (for example a string,
number, array, or ``null``), the member is replaced by an empty object in
``$!`` so that the module metadata can be attached, and the event is accepted
and acknowledged as usual. The original value remains visible in ``msg`` and
``rawmsg``. A ``metadata.imbeats`` member sent by the client is likewise
replaced by the module metadata.

This default may be revisited later. A user-selectable representation mode is
not part of the initial release.


End-to-end Elastic Agent setup
==============================

Install rsyslog, ``imbeats``, and a TLS stream-driver package through your
operating system packages. On Debian or Ubuntu systems using packages that
ship the GnuTLS stream driver separately, a typical TLS prerequisite is:

.. code-block:: bash

   sudo apt install rsyslog rsyslog-gnutls

If your distribution packages ``imbeats.so`` separately, install that package
as well. The exact package name depends on the distribution.

The example below listens on the common Beats/Logstash port ``5044`` and uses
GnuTLS. Replace the certificate paths with files issued for your rsyslog
receiver host.

.. code-block:: none

   module(load="imbeats")

   input(type="imbeats"
         port="5044"
         ruleset="beats_to_file"
         streamdriver.name="gtls"
         streamdriver.mode="1"
         streamdriver.authmode="anon"
         streamdriver.cafile="/etc/rsyslog.d/tls/ca.pem"
         streamdriver.certfile="/etc/rsyslog.d/tls/server-cert.pem"
         streamdriver.keyfile="/etc/rsyslog.d/tls/server-key.pem")

   ruleset(name="beats_to_file") {
     action(type="omfile" file="/var/log/imbeats.log")
   }

Configure Elastic Agent or Filebeat to use the Logstash output and point it at
the rsyslog host:

.. code-block:: yaml

   outputs:
     default:
       type: logstash
       hosts: ["rsyslog.example.net:5044"]
       compression_level: 9
       ssl.enabled: true
       ssl.certificate_authorities:
         - /etc/elastic-agent/certs/ca.pem

For Filebeat standalone configuration, the same output settings are placed
under ``output.logstash``:

.. code-block:: yaml

   output.logstash:
     hosts: ["rsyslog.example.net:5044"]
     compression_level: 9
     ssl.enabled: true
     ssl.certificate_authorities:
       - /etc/filebeat/certs/ca.pem

Use certificate verification in production. Test-only settings such as
``ssl.verification_mode: none`` are useful for isolated lab checks but should
not be used for production ingestion.

The example above lets Elastic Agent or Filebeat validate the rsyslog server
certificate without requiring the sender to present a client certificate. For
mutual TLS, also configure the sender with its own client certificate and key,
then switch the rsyslog listener to the appropriate certificate-validation
auth mode and permitted peer settings.


Troubleshooting Elastic Agent delivery
======================================

- If rsyslog reports that ``gtls`` or ``ossl`` cannot be loaded, install the
  matching TLS stream-driver package, such as ``rsyslog-gnutls`` or
  ``rsyslog-openssl``.
- If the Beat or Agent logs certificate validation errors, confirm that the
  sender trusts the CA that issued the rsyslog listener certificate and that
  the configured host name matches the certificate.
- If the sender connects but does not deliver events, verify that it is using
  ``output.logstash`` and not an Elasticsearch or HTTP output.
- ``imbeats`` accepts compressed Lumberjack v2 frames. Keep compression enabled
  on the sender unless you are isolating a transport problem.
- Use the ``events.received``, ``events.submitted``, ``compressed_frames``, and
  ``protocol_errors`` counters to distinguish traffic, parsing, and protocol
  failures.


Protocol validation and acknowledgements
========================================

Each Lumberjack ``J`` frame payload must contain exactly one complete JSON
object. Leading and trailing JSON whitespace is accepted. Malformed JSON,
arrays, scalar values, concatenated values, and any trailing non-whitespace
bytes are rejected. The original accepted JSON text remains available in
``msg``, while the decoded object fields are mapped into ``$!``.

Malformed frames, resource-limit violations, decompression-limit violations,
and session, frame, or window timeouts fail closed. ``imbeats`` closes the affected
session, releases its charged resources, and does not acknowledge the
incomplete batch.

Lumberjack v2 sequence numbers are scoped to a window. Beats and Elastic Agent
number the events of every window ``1`` to ``n``, and ``imbeats`` accepts that
numbering on every window of a connection. For compatibility with senders that
continue the numbering across windows, a window may instead start at the
sequence number following the last acknowledged one. In both cases the
sequence numbers inside a window must increase by exactly one per event. A
window with any other numbering, for example a gap, a duplicate, or an
out-of-order event, is a protocol error: the session is closed and no event of
that window is submitted or acknowledged.

Once a complete window has been validated and all of its events have been
submitted, ``imbeats`` sends one ``A`` frame carrying the sequence number of
the last event of that window. For Beats clients this equals the number of
events in the window. ``$!metadata!imbeats!sequence`` always contains the
sequence number as sent on the wire.

When a session is closed for a reason other than a normal close by the peer or
rsyslog shutdown, ``imbeats`` logs one message that names the peer address and
port, the local port, the reason, and the rsyslog error code. The input name is
part of the message only when the input's ``name`` parameter is set; inputs
without ``name`` are identified by the local port alone.
Typical reasons are an invalid frame type, a sequence number mismatch, a window
or frame that exceeds the configured limits, a rejected compressed frame, an
event that cannot be submitted, and ``idleTimeout``, ``frameTimeout``, or
``windowTimeout`` expiry. The message never contains payload bytes.

To keep a misbehaving or reconnecting peer from flooding the log, these
messages and the ``too many sessions`` message are rate limited per input and
reason class: at most one message is logged every 10 seconds, and the next
message reports how many similar messages were suppressed. The statistic
counters described below are not rate limited.


Configuration Parameters
========================

Module Parameters
-----------------

Currently none.


Input Parameters
----------------

.. list-table::
   :widths: 34 66
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imbeats-address`
     - .. include:: ../../reference/parameters/imbeats-address.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-frametimeout`
     - .. include:: ../../reference/parameters/imbeats-frametimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-gnutlsprioritystring`
     - .. include:: ../../reference/parameters/imbeats-gnutlsprioritystring.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-idletimeout`
     - .. include:: ../../reference/parameters/imbeats-idletimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-keepalive`
     - .. include:: ../../reference/parameters/imbeats-keepalive.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-keepalive-interval`
     - .. include:: ../../reference/parameters/imbeats-keepalive-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-keepalive-probes`
     - .. include:: ../../reference/parameters/imbeats-keepalive-probes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-keepalive-time`
     - .. include:: ../../reference/parameters/imbeats-keepalive-time.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-listenportfilename`
     - .. include:: ../../reference/parameters/imbeats-listenportfilename.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-maxsessions`
     - .. include:: ../../reference/parameters/imbeats-maxsessions.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-maxbatchbytes`
     - .. include:: ../../reference/parameters/imbeats-maxbatchbytes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-maxcompressionratio`
     - .. include:: ../../reference/parameters/imbeats-maxcompressionratio.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-maxdecompressedsize`
     - .. include:: ../../reference/parameters/imbeats-maxdecompressedsize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-maxframesize`
     - .. include:: ../../reference/parameters/imbeats-maxframesize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-maxinflightbytes`
     - .. include:: ../../reference/parameters/imbeats-maxinflightbytes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-maxwindowsize`
     - .. include:: ../../reference/parameters/imbeats-maxwindowsize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-name`
     - .. include:: ../../reference/parameters/imbeats-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-networknamespace`
     - .. include:: ../../reference/parameters/imbeats-networknamespace.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-permittedpeer`
     - .. include:: ../../reference/parameters/imbeats-permittedpeer.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-port`
     - .. include:: ../../reference/parameters/imbeats-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-ruleset`
     - .. include:: ../../reference/parameters/imbeats-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-starvationprotection-maxreads`
     - .. include:: ../../reference/parameters/imbeats-starvationprotection-maxreads.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-authmode`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-authmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-cafile`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-cafile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-certfile`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-certfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-checkextendedkeypurpose`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-checkextendedkeypurpose.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-crlfile`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-crlfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-keyfile`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-keyfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-mode`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-mode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-name`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-permitexpiredcerts`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-permitexpiredcerts.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-prioritizesan`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-prioritizesan.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-tlsrevocationcheck`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-tlsrevocationcheck.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-streamdriver-tlsverifydepth`
     - .. include:: ../../reference/parameters/imbeats-streamdriver-tlsverifydepth.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-workerthreads`
     - .. include:: ../../reference/parameters/imbeats-workerthreads.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`param-imbeats-windowtimeout`
     - .. include:: ../../reference/parameters/imbeats-windowtimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Examples
========

RainerScript
------------

.. code-block:: none

   module(load="imbeats")

   input(type="imbeats"
         port="5044"
         ruleset="beats_to_es"
         maxInFlightBytes="268435456"
         maxCompressionRatio="256"
         idleTimeout="60"
         frameTimeout="30"
         windowTimeout="300"
         streamdriver.name="gtls"
         streamdriver.mode="1"
         streamdriver.authmode="anon"
         streamdriver.cafile="/etc/rsyslog.d/ca.pem"
         streamdriver.certfile="/etc/rsyslog.d/server-cert.pem"
         streamdriver.keyfile="/etc/rsyslog.d/server-key.pem")

   ruleset(name="beats_to_es") {
     action(type="omfile" file="/var/log/imbeats-debug.log")
   }

YAML
----

.. code-block:: yaml

   version: 2
   modules:
     - load: imbeats

   inputs:
     - type: imbeats
       port: "5044"
       ruleset: beats_to_es
       maxInFlightBytes: 268435456
       maxCompressionRatio: 256
       idleTimeout: 60
       frameTimeout: 30
       windowTimeout: 300
       streamdriver.name: gtls
       streamdriver.mode: 1
       streamdriver.authmode: anon
       streamdriver.cafile: /etc/rsyslog.d/ca.pem
       streamdriver.certfile: /etc/rsyslog.d/server-cert.pem
       streamdriver.keyfile: /etc/rsyslog.d/server-key.pem

   rulesets:
     - name: beats_to_es
       script: |
         action(type="omfile" file="/var/log/imbeats-debug.log")


Statistic Counters
==================

The module exposes these ``impstats`` counters:

- ``connections.accepted``
- ``connections.closed``
- ``protocol_errors``
- ``batches.received``
- ``batches.acked``
- ``events.received``
- ``events.submitted``
- ``events.failed``
- ``compressed_frames``
- ``json_decode_failures``
- ``ack_failures``
- ``windows.rejected``
- ``frames.rejected``
- ``compressed.rejected``
- ``connections.active``
- ``connections.rejected``
- ``starvation_protect``
- ``resource_bytes`` (current bytes charged to the instance resource budget)
- ``resource_rejected``
- ``sessions.timed_out``


.. toctree::
   :hidden:

   ../../reference/parameters/imbeats-address
   ../../reference/parameters/imbeats-frametimeout
   ../../reference/parameters/imbeats-gnutlsprioritystring
   ../../reference/parameters/imbeats-idletimeout
   ../../reference/parameters/imbeats-keepalive
   ../../reference/parameters/imbeats-keepalive-interval
   ../../reference/parameters/imbeats-keepalive-probes
   ../../reference/parameters/imbeats-keepalive-time
   ../../reference/parameters/imbeats-listenportfilename
   ../../reference/parameters/imbeats-maxbatchbytes
   ../../reference/parameters/imbeats-maxcompressionratio
   ../../reference/parameters/imbeats-maxdecompressedsize
   ../../reference/parameters/imbeats-maxframesize
   ../../reference/parameters/imbeats-maxinflightbytes
   ../../reference/parameters/imbeats-maxsessions
   ../../reference/parameters/imbeats-maxwindowsize
   ../../reference/parameters/imbeats-name
   ../../reference/parameters/imbeats-networknamespace
   ../../reference/parameters/imbeats-permittedpeer
   ../../reference/parameters/imbeats-port
   ../../reference/parameters/imbeats-ruleset
   ../../reference/parameters/imbeats-starvationprotection-maxreads
   ../../reference/parameters/imbeats-streamdriver-authmode
   ../../reference/parameters/imbeats-streamdriver-cafile
   ../../reference/parameters/imbeats-streamdriver-certfile
   ../../reference/parameters/imbeats-streamdriver-checkextendedkeypurpose
   ../../reference/parameters/imbeats-streamdriver-crlfile
   ../../reference/parameters/imbeats-streamdriver-keyfile
   ../../reference/parameters/imbeats-streamdriver-mode
   ../../reference/parameters/imbeats-streamdriver-name
   ../../reference/parameters/imbeats-streamdriver-permitexpiredcerts
   ../../reference/parameters/imbeats-streamdriver-prioritizesan
   ../../reference/parameters/imbeats-streamdriver-tlsrevocationcheck
   ../../reference/parameters/imbeats-streamdriver-tlsverifydepth
   ../../reference/parameters/imbeats-workerthreads
   ../../reference/parameters/imbeats-windowtimeout
