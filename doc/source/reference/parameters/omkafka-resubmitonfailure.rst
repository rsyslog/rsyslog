.. _param-omkafka-resubmitonfailure:
.. _omkafka.parameter.module.resubmitonfailure:

resubmitOnFailure
=================

.. index::
   single: omkafka; resubmitOnFailure
   single: resubmitOnFailure

.. summary-start

Retry failed messages when Kafka becomes available.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: resubmitOnFailure
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: 8.28.0

Description
-----------

.. versionadded:: 8.28.0

If enabled, failed messages will be resubmitted automatically when Kafka can
send messages again. Messages rejected because they exceed the maximum size
are dropped.

Action usage
------------

.. _param-omkafka-action-resubmitonfailure:
.. _omkafka.parameter.action.resubmitonfailure:
.. code-block:: rsyslog

   action(type="omkafka" resubmitOnFailure="on")

Notes
-----

- Originally documented as "binary"; uses boolean values.

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

