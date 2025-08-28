.. _param-omkafka-closetimeout:
.. _omkafka.parameter.module.closetimeout:

closeTimeout
============

.. index::
   single: omkafka; closeTimeout
   single: closeTimeout

.. summary-start

Milliseconds to wait for pending messages on shutdown.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: closeTimeout
:Scope: action
:Type: integer
:Default: action=2000
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

Sets the time to wait in ms for draining messages submitted to the
Kafka handle before closing it. The maximum across all actions is used
as librdkafka's unload timeout.

Action usage
------------

.. _param-omkafka-action-closetimeout:
.. _omkafka.parameter.action.closetimeout:
.. code-block:: rsyslog

   action(type="omkafka" closeTimeout="2000")

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

