.. SPDX-License-Identifier: Apache-2.0

.. _param-omclickhouse-compression:
.. _omclickhouse.parameter.action.compression:

.. meta::
   :description: Reference for the omclickhouse compression parameter.
   :keywords: rsyslog, omclickhouse, compression, gzip, clickhouse

compression
===========

.. index::
   single: omclickhouse; compression
   single: compression

.. summary-start

Compresses the HTTP body with gzip when set to gzip.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: compression
:Scope: action
:Type: word
:Default: off
:Required?: no
:Introduced: not specified

Description
-----------
``off`` sends the body as plain text. ``gzip`` compresses it, sets
``Content-Encoding: gzip``, and adds ``enable_http_compression=1`` to the
request URL. The compressed bytes are a gzip stream. Other names are
rejected at configuration time.

The parameter applies to both ``format="values"`` and
``format="jsoneachrow"``.

Action usage
------------
.. _omclickhouse.parameter.action.compression-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" compression="gzip")

YAML usage
----------

.. code-block:: yaml

   actions:
     - type: omclickhouse
       format: jsoneachrow
       table: rsyslog.events
       template: ClickHouseJson
       compression: gzip

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
