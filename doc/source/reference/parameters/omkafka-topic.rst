.. _param-omkafka-topic:
.. _omkafka.parameter.module.topic:

Topic
=====

.. index::
   single: omkafka; Topic
   single: Topic

.. summary-start

Kafka topic to produce to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: Topic
:Scope: action
:Type: string
:Default: action=none
:Required?: yes
:Introduced: at least 8.x, possibly earlier

Description
-----------

Specifies the topic to produce to.

Action usage
------------

.. _param-omkafka-action-topic:
.. _omkafka.parameter.action.topic:
.. code-block:: rsyslog

   action(type="omkafka" Topic="mytopic")

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

