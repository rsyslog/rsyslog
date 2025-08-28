.. _param-omkafka-partitions-auto:
.. _omkafka.parameter.module.partitions-auto:

Partitions.Auto
===============

.. index::
   single: omkafka; Partitions.Auto
   single: Partitions.Auto

.. summary-start

Use librdkafka's automatic partitioning.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: Partitions.Auto
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

Librdkafka provides an automatic partitioning function that distributes
messages into all partitions configured for a topic.

Action usage
------------

.. _param-omkafka-action-partitions-auto:
.. _omkafka.parameter.action.partitions-auto:
.. code-block:: rsyslog

   action(type="omkafka" Partitions.Auto="on")

Notes
-----

- Originally documented as "binary"; uses boolean values.
- Overrides other partitioning settings when enabled.

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

