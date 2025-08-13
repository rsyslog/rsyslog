.. _param-omkafka-key:
.. _omkafka.parameter.module.key:

Key
===

.. index::
   single: omkafka; Key
   single: Key

.. summary-start

Key used for partitioning messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: Key
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

Kafka key to be used for all messages. If a key is provided and
``partitions.auto="on"`` is set, then all messages will be assigned to
a partition based on the key.

Action usage
------------

.. _param-omkafka-action-key:
.. _omkafka.parameter.action.key:
.. code-block:: rsyslog

   action(type="omkafka" Key="mykey")

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

