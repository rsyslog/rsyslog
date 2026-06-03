.. _param-imkafka-topic:
.. _imkafka.parameter.input.topic:

topic
=====

.. index::
   single: imkafka; topic
   single: topic

.. summary-start

Identifies one or more Kafka topics from which imkafka consumes messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkafka`.

:Name: topic
:Scope: input
:Type: array
:Required?: yes
:Introduced: 8.27.0

Description
-----------
Specifies the Kafka topic or topics to consume from. A single topic name may
be given as a string. Multiple topics may be given as an array, allowing one
``input()`` to consume from several topics at once.

Input usage
-----------
.. _imkafka.parameter.input.topic-usage:

Single topic:

.. code-block:: rsyslog

   module(load="imkafka")
   input(type="imkafka"
         topic="mytopic"
         broker="localhost:9092"
         consumergroup="default")

Multiple topics:

.. code-block:: rsyslog

   module(load="imkafka")
   input(type="imkafka"
         topic=["topic1", "topic2", "topic3"]
         broker="localhost:9092"
         consumergroup="default")

See also
--------
See also :doc:`../../configuration/modules/imkafka`.
