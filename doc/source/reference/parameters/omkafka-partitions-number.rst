.. _param-omkafka-partitions-number:
.. _omkafka.parameter.module.partitions-number:

Partitions.number
=================

.. index::
   single: omkafka; Partitions.number
   single: Partitions.number

.. summary-start

Number of partitions to load-balance across.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: Partitions.number
:Scope: action
:Type: integer
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

If set, specifies how many partitions exist and activates load-balancing among them.
The number must match the topic's partition count.

Action usage
------------

.. _param-omkafka-action-partitions-number:
.. _omkafka.parameter.action.partitions-number:
.. code-block:: rsyslog

   action(type="omkafka" Partitions.number="3")

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

