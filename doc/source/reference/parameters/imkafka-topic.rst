.. _param-imkafka-topic:
.. _imkafka.parameter.input.topic:

Topic
=====

.. index::
   single: imkafka; Topic
   single: Topic

.. summary-start

Identifies the Kafka topic from which imkafka consumes messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkafka`.

:Name: Topic
:Scope: input
:Type: string
:Default: input=``none``
:Required?: yes
:Introduced: 8.27.0

Description
-----------
Specifies the topic to produce to.

Input usage
-----------
.. _param-imkafka-input-topic:
.. _imkafka.parameter.input.topic-usage:

.. code-block:: rsyslog

   module(load="imkafka")
   input(type="imkafka" topic="static")

See also
--------
See also :doc:`../../configuration/modules/imkafka`.
