.. _param-omkafka-kafkaheader:
.. _omkafka.parameter.module.kafkaheader:

KafkaHeader
===========

.. index::
   single: omkafka; KafkaHeader
   single: KafkaHeader

.. summary-start

Defines Kafka message headers to attach to each produced message. Each entry is a `key=value` pair.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: KafkaHeader
:Scope: action
:Type: array[string]
:Default: action=none
:Required?: no
:Introduced: 8.2405.0

Description
-----------

Specifies static headers that are added to every message sent to Kafka. The parameter accepts an array of `key=value` strings.

Action usage
------------

.. code-block:: rsyslog

   action(type="omkafka"
          topic="mytopic"
          broker="localhost:9092"
          kafkaHeader=["foo=bar","answer=42"])

See also
--------

* :doc:`../../configuration/modules/omkafka`
