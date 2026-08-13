.. _rsyslog-segqueue:

=================
rsyslog-segqueue
=================

.. meta::
   :description: Inspect, export, and safely repair experimental segmentedDisk queue stores.
   :keywords: rsyslog, segmentedDisk, queue, repair, recovery, JSON Lines

.. summary-start

``rsyslog-segqueue`` is an offline maintenance utility for version 2
``segmentedDisk`` stores. It provides fast status, complete integrity checks,
lossless JSONL export, conservative state reconstruction, and salvage repair.

.. summary-end

Use this tool on the dedicated
``<workDirectory>/<queue.filename>.segq/`` directory. It does not support
classic ``.qi`` queues or the rejected experimental version 1 format.

Safety model
============

Status, check, and export are read-only. Stop rsyslog before applying any
repair: the utility can detect files that change during preparation, but the
daemon and utility do not share a lock protocol.

Repair runs in plan-only mode unless both ``--apply`` and ``--offline`` are
present. A state-slot repair retains a timestamped copy of the old state file.
Rebuild and salvage create and validate a sibling store, rename the original to
a timestamped backup, and atomically install the replacement. No backup is
removed automatically.

When state cannot prove which records were committed, rebuild and salvage make
every recovered record pending. This can produce duplicates, but avoids
silently discarding recoverable log records.

Commands
========

``status QUEUE-DIR [--json]``
   Read state slots and segment metadata without scanning record payloads.

``check QUEUE-DIR [--json]``
   Validate the state file, topology, segment headers and footers, record
   framing and checksums, and every persisted TLV message.

``export QUEUE-DIR [--output FILE|-] [--scope live|all] [--salvage]``
   Write one JSON object per record. ``live`` is the default scope and begins
   at the durable commit frontier. ``all`` includes already committed physical
   data. Corruption is fatal unless ``--salvage`` is selected.

``repair QUEUE-DIR --mode state-slot|rebuild|salvage``
   Produce a no-write plan. Add ``--apply --offline`` to apply it.
   ``state-slot`` restores redundant state, ``rebuild`` requires clean segment
   data, and ``salvage`` copies every independently verified record into a
   normalized store.

``tui [QUEUE-DIR]``
   Open a guided, dependency-free terminal menu over the same operations.

JSONL schema
============

Each line contains a ``queue`` object with the source segment, byte offsets,
and local record sequence, plus a ``message`` object containing every persisted
message property. ``msg`` and ``rawmsg_after_pri`` are derived from the stored
offsets. Embedded message JSON and local variables remain nested JSON values.
An unparsed message may carry rsyslog's unsigned ``0xffffffff`` offset sentinel;
in that case ``msg`` contains the complete raw message and
``msg_offset_unset`` is true.

Text fields are JSON strings when they contain valid UTF-8. Otherwise the field
is an object such as ``{"encoding":"base64","data":"..."}``, preserving the
original bytes without replacement characters.

Exit codes
==========

``0``
   Success with no integrity errors.

``1``
   Invalid invocation, filesystem failure, or internal processing error.

``2``
   Integrity errors were found, or salvage export completed with omissions.

Examples
========

.. code-block:: bash

   rsyslog-segqueue status /var/spool/rsyslog/mainq.segq
   rsyslog-segqueue check --json /var/spool/rsyslog/mainq.segq
   rsyslog-segqueue export /var/spool/rsyslog/mainq.segq --output backlog.jsonl
   rsyslog-segqueue repair /var/spool/rsyslog/mainq.segq --mode salvage
   rsyslog-segqueue repair /var/spool/rsyslog/mainq.segq \
     --mode salvage --apply --offline

See also
========

See :doc:`../../concepts/queues` for segmented queue behavior and durability
semantics.
