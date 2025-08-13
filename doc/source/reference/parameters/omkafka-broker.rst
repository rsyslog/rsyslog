.. _param-omkafka-broker:
.. _omkafka.parameter.module.broker:

Broker
======

.. index::
   single: omkafka; Broker
   single: Broker

.. summary-start

List of Kafka brokers in `host:port` form.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: Broker
:Scope: action
:Type: array[string]
:Default: action=localhost:9092
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

Specifies the broker(s) to use.

Action usage
------------

.. _param-omkafka-action-broker:
.. _omkafka.parameter.action.broker:
.. code-block:: rsyslog

   action(type="omkafka" Broker=["host1:9092","host2:9092"])

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

