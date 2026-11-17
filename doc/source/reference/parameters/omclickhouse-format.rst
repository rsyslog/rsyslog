.. SPDX-License-Identifier: Apache-2.0

.. _param-omclickhouse-format:
.. _omclickhouse.parameter.action.format:

.. meta::
   :description: Reference for the omclickhouse format parameter.
   :keywords: rsyslog, omclickhouse, format, jsoneachrow, values, clickhouse

format
======

.. index::
   single: omclickhouse; format
   single: format

.. summary-start

Selects a rendered INSERT statement or newline-delimited JSONEachRow.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: format
:Scope: action
:Type: word
:Default: values
:Required?: no
:Introduced: not specified

Description
-----------
``values`` posts the rendered template as the HTTP body. That body must
contain ``INSERT INTO``. In bulk mode the module still joins statements on
the ``VALUES`` clause. This is the historical behavior.

``jsoneachrow`` posts one JSON object per event, separated by newlines,
with ``Content-Type: application/x-ndjson``. The INSERT statement is the
``query`` URL parameter: ``INSERT INTO <table> FORMAT JSONEachRow``. A
template is required. A list template with ``option.jsonf="on"`` renders
one escaped object per event. ``option.sql``, ``option.stdsql``, and
``option.json`` are rejected because they would escape the object's own
quotes. ``table`` is required.

Action usage
------------
.. _omclickhouse.parameter.action.format-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   template(name="ClickHouseJson" type="list" option.jsonf="on") {
       property(outname="message" name="msg" format="jsonf")
   }
   action(type="omclickhouse" format="jsoneachrow" table="rsyslog.events"
          template="ClickHouseJson")

YAML usage
----------

.. code-block:: yaml

   actions:
     - type: omclickhouse
       format: jsoneachrow
       table: rsyslog.events
       template: ClickHouseJson

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
