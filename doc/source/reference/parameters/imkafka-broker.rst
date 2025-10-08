.. _param-imkafka-broker:
.. _imkafka.parameter.input.broker:

broker
======

.. index::
   single: imkafka; broker
   single: broker

.. summary-start

Selects the Kafka broker(s) imkafka connects to when consuming messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkafka`.

:Name: broker
:Scope: input
:Type: array
:Default: input=``localhost:9092``
:Required?: no
:Introduced: 8.27.0

Description
-----------
Specifies the broker or brokers to use.

Input usage
-----------
.. _imkafka.parameter.input.broker-usage:

.. code-block:: rsyslog

   module(load="imkafka")
   input(type="imkafka"
         topic="your-topic"
         broker=["localhost:9092", "localhost:9093"])

See also
--------
See also :doc:`../../configuration/modules/imkafka`.
