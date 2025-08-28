.. _param-omkafka-partitions-usefixed:
.. _omkafka.parameter.module.partitions-usefixed:

Partitions.useFixed
===================

.. index::
   single: omkafka; Partitions.useFixed
   single: Partitions.useFixed

.. summary-start

Send all messages to a specific partition.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: Partitions.useFixed
:Scope: action
:Type: integer
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

If set, specifies the partition to which data is produced. All data goes to
this partition; no other partition is used.

Action usage
------------

.. _param-omkafka-action-partitions-usefixed:
.. _omkafka.parameter.action.partitions-usefixed:
.. code-block:: rsyslog

   action(type="omkafka" Partitions.useFixed="1")

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

