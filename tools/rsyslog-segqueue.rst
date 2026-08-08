=================
rsyslog-segqueue
=================

-------------------------------------------
Maintain segmentedDisk queue stores offline
-------------------------------------------

:Manual section: 1

SYNOPSIS
========

::

   rsyslog-segqueue status QUEUE-DIR [--json]
   rsyslog-segqueue check QUEUE-DIR [--json]
   rsyslog-segqueue export QUEUE-DIR [--output FILE|-]
                           [--scope live|all] [--salvage]
   rsyslog-segqueue repair QUEUE-DIR --mode state-slot|rebuild|salvage
                           [--apply --offline] [--json]
   rsyslog-segqueue tui [QUEUE-DIR]

DESCRIPTION
===========

``rsyslog-segqueue`` inspects, exports, and repairs the version 2 store used by
``queue.type="segmentedDisk"``.  It does not operate on classic ``.qi`` disk
queues.  The queue directory is normally
``<workDirectory>/<queue.filename>.segq``.

Status, check, and export are read-only.  They detect a store that changes
during repair preparation, but cannot provide a coherent snapshot while
rsyslog is writing the queue.  Stop rsyslog before applying any repair.

COMMANDS
========

status
------

Read state slots and segment metadata.  The report includes the selected state
generation, queue UUID, commit frontier, live/recovery/delete ranges, physical
size, and immediately visible inconsistencies.  Payloads are not scanned.

check
-----

Scan the complete store.  Checks cover state-slot checksums and invariants,
segment headers and footers, topology, record framing and sequence numbers,
payload checksums, and the versioned TLV message codec.  Every error includes a
stable code and location.

export
------

Write one JSON object per valid record.  The default ``--scope live`` begins at
the durable commit frontier.  ``--scope all`` also includes physical records
that state identifies as already committed or pending deletion.

Export refuses a corrupt store by default.  ``--salvage`` exports independently
valid records, reports omissions, and exits with status 2.  Valid UTF-8 is
written as a JSON string.  Other byte strings are represented as an object with
``encoding`` set to ``base64`` and a lossless ``data`` value.
For an unparsed message whose stored message offset is rsyslog's
``0xffffffff`` sentinel, ``msg`` contains the complete raw message and
``msg_offset_unset`` is true.

repair
------

Repair is a no-write plan unless both ``--apply`` and ``--offline`` are given.
The modes are:

``state-slot``
  Recreate one invalid state slot from the surviving valid slot and preserve
  its commit frontier.

``rebuild``
  Require clean, unambiguous segment data and construct a normalized store in
  which every recovered record is pending.

``salvage``
  Resynchronize after corrupt framing, retain every record whose framing,
  payload checksum, and codec validate, and construct a normalized store.

Rebuild and salvage may replay records that had already been processed because
an unusable state file cannot prove the old commit frontier.  This deliberate
duplicate-over-loss policy is always shown in the repair plan.

Applied repairs retain the original state file or queue directory under a
UTC-timestamped ``backup`` name.  Rebuild and salvage validate a same-filesystem
staging store before atomically renaming the original.  Backups are never
deleted automatically.

tui
---

Open a dependency-free guided terminal menu.  It invokes the same commands and
requires the exact queue path to be typed before applying a repair.

OPTIONS
-------

``--json``
  Emit a machine-readable status, check, or repair report.

``--output FILE|-``
  Write JSON Lines to FILE.  ``-`` selects standard output and is the default.

``--scope live|all``
  Select logically live records or all physical records.  The default is
  ``live``.

``--salvage``
  Permit a partial JSONL export from a corrupt store.

``--apply``
  Apply a repair plan.  This also requires ``--offline``.

``--offline``
  Acknowledge that rsyslog is stopped and will not access the queue during the
  repair.

EXIT STATUS
===========

0
  The command completed successfully and no integrity errors were found.

1
  Invocation, filesystem, or internal processing failed.

2
  Integrity errors were found, or a salvage export completed with omissions.

EXAMPLES
========

::

   rsyslog-segqueue status /var/spool/rsyslog/mainq.segq
   rsyslog-segqueue check --json /var/spool/rsyslog/mainq.segq
   rsyslog-segqueue export /var/spool/rsyslog/mainq.segq --output backlog.jsonl
   rsyslog-segqueue repair /var/spool/rsyslog/mainq.segq --mode salvage
   rsyslog-segqueue repair /var/spool/rsyslog/mainq.segq --mode salvage --apply --offline

SEE ALSO
========

``rsyslogd(8)``, ``rsyslog.conf(5)``
