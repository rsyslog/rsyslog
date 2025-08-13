.. _param-omkafka-topicconfparam:
.. _omkafka.parameter.module.topicconfparam:

TopicConfParam
==============

.. index::
   single: omkafka; TopicConfParam
   single: TopicConfParam

.. summary-start

Arbitrary librdkafka topic options `name=value`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: TopicConfParam
:Scope: action
:Type: array[string]
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

In essence the same as ``ConfParam``, but for Kafka topic options.

Action usage
------------

.. _param-omkafka-action-topicconfparam:
.. _omkafka.parameter.action.topicconfparam:
.. code-block:: rsyslog

   action(type="omkafka" topicConfParam=["acks=all"])

Notes
-----

- Multiple values may be set using array syntax.

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

