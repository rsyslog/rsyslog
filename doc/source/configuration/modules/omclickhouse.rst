.. SPDX-License-Identifier: Apache-2.0

**************************************
omclickhouse: ClickHouse Output Module
**************************************

===========================  ===========================================================================
**Module Name:**             **omclickhouse**
**Author:**                  Pascal Withopf <pwithopf@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module provides native support for logging to
`ClickHouse <https://clickhouse.yandex/>`_.
To enable the module use "--enable-clickhouse" while configuring rsyslog.
Tests for the testbench can be enabled with "--enable-clickhouse-tests".

The module supports two insert formats, selected with
:ref:`param-omclickhouse-format`. With ``values``, the default, the template
renders a complete ``INSERT ... VALUES`` statement. With ``jsoneachrow``, the
template renders one JSON object per message and the module builds the
``INSERT ... FORMAT JSONEachRow`` statement for :ref:`param-omclickhouse-table`.


Notable Features
================

- JSONEachRow inserts (:ref:`param-omclickhouse-format`), with optional
  :ref:`gzip compression <param-omclickhouse-compression>` and
  :ref:`asynchronous inserts <param-omclickhouse-asyncinsert>`
- :ref:`omclickhouse-error-handling`
- :ref:`omclickhouse-statistic-counter`


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.


Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omclickhouse-allowunsignedcerts`
     - .. include:: ../../reference/parameters/omclickhouse-allowunsignedcerts.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-asyncinsert`
     - .. include:: ../../reference/parameters/omclickhouse-asyncinsert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-bulkmode`
     - .. include:: ../../reference/parameters/omclickhouse-bulkmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-compression`
     - .. include:: ../../reference/parameters/omclickhouse-compression.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-errorfile`
     - .. include:: ../../reference/parameters/omclickhouse-errorfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-format`
     - .. include:: ../../reference/parameters/omclickhouse-format.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-healthchecktimeout`
     - .. include:: ../../reference/parameters/omclickhouse-healthchecktimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-maxbytes`
     - .. include:: ../../reference/parameters/omclickhouse-maxbytes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-maxconsecutiverejects`
     - .. include:: ../../reference/parameters/omclickhouse-maxconsecutiverejects.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-port`
     - .. include:: ../../reference/parameters/omclickhouse-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-pwd`
     - .. include:: ../../reference/parameters/omclickhouse-pwd.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-server`
     - .. include:: ../../reference/parameters/omclickhouse-server.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-skipverifyhost`
     - .. include:: ../../reference/parameters/omclickhouse-skipverifyhost.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-table`
     - .. include:: ../../reference/parameters/omclickhouse-table.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-template`
     - .. include:: ../../reference/parameters/omclickhouse-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-timeout`
     - .. include:: ../../reference/parameters/omclickhouse-timeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-usehttps`
     - .. include:: ../../reference/parameters/omclickhouse-usehttps.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omclickhouse-user`
     - .. include:: ../../reference/parameters/omclickhouse-user.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/omclickhouse-allowunsignedcerts
   ../../reference/parameters/omclickhouse-asyncinsert
   ../../reference/parameters/omclickhouse-bulkmode
   ../../reference/parameters/omclickhouse-compression
   ../../reference/parameters/omclickhouse-errorfile
   ../../reference/parameters/omclickhouse-format
   ../../reference/parameters/omclickhouse-healthchecktimeout
   ../../reference/parameters/omclickhouse-maxbytes
   ../../reference/parameters/omclickhouse-maxconsecutiverejects
   ../../reference/parameters/omclickhouse-port
   ../../reference/parameters/omclickhouse-pwd
   ../../reference/parameters/omclickhouse-server
   ../../reference/parameters/omclickhouse-skipverifyhost
   ../../reference/parameters/omclickhouse-table
   ../../reference/parameters/omclickhouse-template
   ../../reference/parameters/omclickhouse-timeout
   ../../reference/parameters/omclickhouse-usehttps
   ../../reference/parameters/omclickhouse-user


.. _omclickhouse-error-handling:

Error Handling
==============

omclickhouse sorts a failed insert by the ClickHouse exception code. ClickHouse
sends it in the ``X-ClickHouse-Exception-Code`` header and at the start of the
reply (``Code: N.``). The HTTP status alone is not enough: a violated CHECK
constraint is HTTP 500 and a bad token in a ``VALUES`` row is HTTP 404.

- Codes caused by the content of a row are data errors: the ``CANNOT_PARSE_*``
  family, ``INCORRECT_DATA``, ``TYPE_MISMATCH``, ``CANNOT_CONVERT_TYPE``,
  ``VALUE_IS_OUT_OF_RANGE_OF_DATA_TYPE``,
  ``CANNOT_INSERT_NULL_IN_ORDINARY_COLUMN``, ``TOO_LARGE_STRING_SIZE``,
  ``VIOLATED_CONSTRAINT``, and, for ``VALUES`` rows, ``SYNTAX_ERROR`` and
  ``UNKNOWN_IDENTIFIER``. A reply without a code is a data error when its
  status is 400 or 413.
- When a bulk batch fails with a data error, omclickhouse sends its rows
  again one at a time. Rows that ClickHouse accepts are inserted. Rows it
  still rejects are written to :ref:`param-omclickhouse-errorfile`, if set,
  and dropped, so one bad record does not block the queue. ClickHouse inserts
  all rows of a request or none, so this does not insert a row twice. A
  request sent early because the batch reached
  :ref:`param-omclickhouse-maxbytes` is handled the same way. A row is
  dropped only after its error-file record is written; if the file cannot be
  written, the action suspends and keeps the row queued. With
  ``format="values"``, a row that is not an ``INSERT`` statement is rejected
  the same way without being sent.
- :ref:`param-omclickhouse-maxconsecutiverejects` limits how many rows are
  dropped in a row. A schema change or a broken template rejects every row;
  past the limit the action suspends instead of dropping them.
- Every other failure suspends the action: connection errors, a missing
  table or column, wrong credentials, a read-only user, an unknown setting,
  memory or part limits, and server errors. A single such failure is retried
  at once; after two in a row the action waits ``action.resumeInterval``
  before it checks the server again, even though a plain connection check
  would succeed.
- ``action.resumeRetryCount`` decides when a suspended batch is discarded.
  With the default ``"0"`` the engine discards it. With a positive count it
  discards it after that many failed connection checks; with ``"-1"`` it
  never does. When ClickHouse answers the connection check but keeps failing
  the insert, each successful check starts a new retry cycle, so a positive
  count does not run out: the rows are retried every
  ``action.resumeInterval`` until ClickHouse accepts them. Give the action a
  disk-assisted queue
  (``queue.type`` and ``queue.filename``) when events must survive a long
  outage or a restart.
- Delivery is at least once. When a message suspends the action in the
  middle of a transaction, the rsyslog core retries the transaction from its
  first message, so a message that was already inserted, or already written
  to the error file, can be sent again. A request sent early because of
  ``maxBytes`` that fails for any reason other than bad data keeps its rows,
  and they are sent with the rest of the transaction.


.. _omclickhouse-statistic-counter:

Statistic Counter
=================

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>`,
which accumulate all action instances. The statistic is named "omclickhouse".
Parameters are:

-  **submitted** - number of messages submitted for processing (with both
   success and error result)

-  **fail.httprequests** - the number of times an HTTP request failed. Note
   that a single HTTP request may be used to submit multiple messages, so this
   number may be (much) lower than failed.http.

-  **failed.http** - number of message failures due to connection like-problems
   (things like remote server down, broken link etc)

-  **fail.clickhouse** - number of failures due to clickhouse error reply; Note that
   this counter does NOT count the number of failed messages but the number of
   times a failure occurred (a potentially much smaller number). Counting messages
   would be quite performance-intense and is thus not done.

-  **response.success** - number of records successfully sent in bulk index
   requests - counts the number of successful responses


**The fail.httprequests and failed.http counters reflect only failures that
omclickhouse detected.** Once it detects problems, it (usually, depends on
circumstances) tell the rsyslog core that it wants to be suspended until the
situation clears (this is a requirement for rsyslog output modules). Once it is
suspended, it does NOT receive any further messages. Depending on the user
configuration, messages will be lost during this period. Those lost messages will
NOT be counted by impstats (as it does not see them).


Examples
========

Example 1
---------

The following sample does the following:

-  loads the omclickhouse module
-  outputs all logs to ClickHouse using the default settings

.. code-block:: none

    module(load="omclickhouse")
    action(type="omclickhouse")


Example 2
---------

In this example the URL will use `http` and the specified parameters to create
the REST URL.

.. code-block:: none

   module(load="omclickhouse")
   action(type="omclickhouse" server="127.0.0.1" port="8124" user="user1" pwd="pwd1"
          useHttps="off")


Example 3
---------

This example sends messages in batches of up to 10MB. Rows that ClickHouse
rejects are written to the error file.

.. code-block:: none

   module(load="omclickhouse")
   action(type="omclickhouse" maxBytes="10mb" errorFile="clickhouse-error.log")


Example 4
---------

This example posts events as JSONEachRow. The ``option.jsonf`` list template
renders one escaped JSON object per event. ``asyncInsert`` and gzip apply only
because they are set here. Rows that ClickHouse rejects are written to the
error file. Other failures suspend the action until the batch can be retried.

.. code-block:: rsyslog

   module(load="omclickhouse")
   template(name="ClickHouseJson" type="list" option.jsonf="on") {
       property(outname="message" name="msg" format="jsonf")
       property(outname="host" name="hostname" format="jsonf")
   }
   action(type="omclickhouse"
          server="127.0.0.1"
          useHttps="off"
          format="jsoneachrow"
          table="rsyslog.events"
          template="ClickHouseJson"
          asyncInsert="on"
          compression="gzip"
          errorFile="/var/log/clickhouse-error.json")


