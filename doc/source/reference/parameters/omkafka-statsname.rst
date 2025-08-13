.. _param-omkafka-statsname:
.. _omkafka.parameter.module.statsname:

statsName
=========

.. index::
   single: omkafka; statsName
   single: statsName

.. summary-start

Name of statistics instance for this action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: statsName
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: 8.2108.0

Description
-----------

.. versionadded:: 8.2108.0

The name assigned to statistics specific to this action instance. Counters
include ``submitted``, ``acked`` and ``failures``.

Action usage
------------

.. _param-omkafka-action-statsname:
.. _omkafka.parameter.action.statsname:
.. code-block:: rsyslog

   action(type="omkafka" statsName="producer-1")

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

