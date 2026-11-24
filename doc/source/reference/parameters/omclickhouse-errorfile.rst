.. SPDX-License-Identifier: Apache-2.0

.. _param-omclickhouse-errorfile:
.. _omclickhouse.parameter.input.errorfile:

errorFile
=========

.. index::
   single: omclickhouse; errorFile
   single: errorFile
   single: omclickhouse; errorfile
   single: errorfile

.. summary-start

Records each request that ClickHouse rejects as bad data, with the reply.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: errorFile
:Scope: input
:Type: word
:Default: none
:Required?: no
:Introduced: not specified

Description
-----------
If specified, each request whose data ClickHouse finally rejects appends one
JSON object to this file. The object holds the request URL and body
(``request.url`` and ``request.postdata``), the HTTP status
(``http_status``), and the ClickHouse reply (``reply``). The file is kept
at mode 0600 because it contains event data. Rsyslog does not read the file
again. A script can inspect it and resubmit the records once the cause is
fixed.

.. note::
   A data error is a ClickHouse exception code caused by the content of a row
   (for example ``CANNOT_PARSE_INPUT_ASSERTION_FAILED``, ``INCORRECT_DATA``,
   or ``VIOLATED_CONSTRAINT``), or HTTP status 400 or 413 when the reply has
   no code. See :ref:`omclickhouse-error-handling`. When a bulk batch fails
   with a data error, nothing is written yet: omclickhouse sends its rows
   again one at a time. Rows that ClickHouse accepts are inserted. Rows it
   still rejects are written here and dropped, so one bad record does not
   block the queue. Without bulk mode each message is its own request and a
   rejection is written at once.

   With ``format="values"``, a row that is not an ``INSERT`` statement is
   rejected by omclickhouse itself and written here with ``http_status`` 0.

   A row is dropped only after its record is written. If this file cannot be
   opened or written, the action suspends and keeps the row queued until the
   record succeeds. Rows held back by
   :ref:`param-omclickhouse-maxconsecutiverejects` are not written here; they
   stay queued. Every other failure suspends the action,
   keeps the batch queued, and is not written here.

Input usage
-----------
.. _omclickhouse.parameter.input.errorfile-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" errorFile="clickhouse-error.log")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
