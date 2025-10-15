.. _param-impstats-format:
.. _impstats.parameter.module.format:

format
======

.. index::
   single: impstats; format
   single: format

.. summary-start

Specifies which output format impstats uses for emitted statistics records.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: format
:Scope: module
:Type: word
:Default: module=legacy
:Required?: no
:Introduced: 8.16.0

Description
-----------
.. versionadded:: 8.16.0

Specifies the format of emitted stats messages. The default of ``legacy`` is
compatible with pre v6-rsyslog. The other options provide support for structured
formats (note that ``cee`` is actually "project lumberjack" logging).

The ``json-elasticsearch`` format supports the broken ElasticSearch JSON
implementation. ES 2.0 no longer supports valid JSON and disallows dots inside
names. The ``json-elasticsearch`` format option replaces those dots by the bang
("!") character. So ``discarded.full`` becomes ``discarded!full``.

Options: ``json`` / ``json-elasticsearch`` / ``cee`` / ``legacy`` / ``prometheus``.

Module usage
------------
.. _impstats.parameter.module.format-usage:

.. code-block:: rsyslog

   module(load="impstats" format="json")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
