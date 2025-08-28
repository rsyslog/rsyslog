.. _param-omkafka-statsfile:
.. _omkafka.parameter.module.statsfile:

statsFile
=========

.. index::
   single: omkafka; statsFile
   single: statsFile

.. summary-start

Write librdkafka statistics JSON to this file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: statsFile
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

If set, the JSON statistics from librdkafka are written to the specified file.
Updates occur based on the ``statistics.interval.ms`` `confParam` value.

Action usage
------------

.. _param-omkafka-action-statsfile:
.. _omkafka.parameter.action.statsfile:
.. code-block:: rsyslog

   action(type="omkafka" statsFile="/var/log/omkafka-stats.json")

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

