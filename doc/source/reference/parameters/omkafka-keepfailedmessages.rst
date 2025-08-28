.. _param-omkafka-keepfailedmessages:
.. _omkafka.parameter.module.keepfailedmessages:

KeepFailedMessages
==================

.. index::
   single: omkafka; KeepFailedMessages
   single: KeepFailedMessages

.. summary-start

Persist failed messages for resending after restart.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: KeepFailedMessages
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

If enabled, failed messages will be saved and loaded on shutdown/startup and
resent after startup when Kafka is available. This setting requires
``resubmitOnFailure`` to be enabled.

Action usage
------------

.. _param-omkafka-action-keepfailedmessages:
.. _omkafka.parameter.action.keepfailedmessages:
.. code-block:: rsyslog

   action(type="omkafka" KeepFailedMessages="on")

Notes
-----

- Originally documented as "binary"; uses boolean values.

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

