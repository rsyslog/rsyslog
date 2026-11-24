.. SPDX-License-Identifier: Apache-2.0

.. _param-omclickhouse-table:
.. _omclickhouse.parameter.action.table:

.. meta::
   :description: Reference for the omclickhouse table parameter.
   :keywords: rsyslog, omclickhouse, table, jsoneachrow, clickhouse

table
=====

.. index::
   single: omclickhouse; table
   single: table

.. summary-start

ClickHouse table used by format jsoneachrow. Required in that mode.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: table
:Scope: action
:Type: string
:Default: none
:Required?: no
:Introduced: not specified

Description
-----------
Required with ``format="jsoneachrow"`` and used only there. The value is placed in
``INSERT INTO <table> FORMAT JSONEachRow``. It must be a table name or
``database.table``. Each part starts with an ASCII letter or underscore
and contains only letters, digits, and underscore. One dot is allowed
between the database and the table. The
maximum length is 128 characters. Anything else is rejected so the value
cannot change the query string.

Setting ``table`` together with ``format="values"`` is a configuration
error. In that mode the INSERT statement comes from the template.

Action usage
------------
.. _omclickhouse.parameter.action.table-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   template(name="ClickHouseJson" type="list" option.jsonf="on") {
       property(outname="message" name="msg" format="jsonf")
   }
   action(type="omclickhouse" format="jsoneachrow" table="soc.events"
          template="ClickHouseJson")

YAML usage
----------

.. code-block:: yaml

   actions:
     - type: omclickhouse
       format: jsoneachrow
       table: soc.events
       template: ClickHouseJson

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
