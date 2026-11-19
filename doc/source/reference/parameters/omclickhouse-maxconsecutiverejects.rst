.. SPDX-License-Identifier: Apache-2.0

.. _param-omclickhouse-maxconsecutiverejects:
.. _omclickhouse.parameter.action.maxconsecutiverejects:

.. meta::
   :description: Reference for the omclickhouse maxConsecutiveRejects parameter.
   :keywords: rsyslog, omclickhouse, maxconsecutiverejects, data error, schema change, clickhouse

maxConsecutiveRejects
=====================

.. index::
   single: omclickhouse; maxConsecutiveRejects
   single: maxConsecutiveRejects

.. summary-start

Stops dropping rejected rows after this many in a row, so a schema change
keeps events queued.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: maxConsecutiveRejects
:Scope: action
:Type: non-negative integer
:Default: 0
:Required?: no
:Introduced: not specified

Description
-----------
A row that ClickHouse rejects as bad data is written to
:ref:`param-omclickhouse-errorfile` and dropped, so it does not block the
rows after it. When every row is rejected, the cause is usually not the data
but the table or the template, for example a column whose type changed.
Dropping then empties the queue into the error file.

With a value above 0, the action counts final rejections since the last
accepted insert. Once the count reaches this value, the next rejected row is
not dropped: the action suspends. With ``action.resumeRetryCount="0"`` the
engine discards the batch; with any other value, while ClickHouse answers its
connection check, it retries the batch every ``action.resumeInterval`` until
ClickHouse accepts it.
The rows after it stay queued. The first accepted insert resets the count and
rejected rows are dropped again.

``0``, the default, never stops dropping. Each worker keeps its own count.
Size the action queue for the time a schema problem may take to fix.

Action usage
------------
.. _omclickhouse.parameter.action.maxconsecutiverejects-usage:

.. code-block:: rsyslog

   action(type="omclickhouse" maxConsecutiveRejects="100"
          errorFile="/var/log/clickhouse-error.json")

YAML usage
----------

.. code-block:: yaml

   actions:
     - type: omclickhouse
       maxconsecutiverejects: 100
       errorfile: /var/log/clickhouse-error.json

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
