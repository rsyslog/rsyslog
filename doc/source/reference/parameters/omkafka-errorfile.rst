.. _param-omkafka-errorfile:
.. _omkafka.parameter.module.errorfile:

errorFile
=========

.. index::
   single: omkafka; errorFile
   single: errorFile

.. summary-start

Write failed messages to this JSON file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: errorFile
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

If set, messages that could not be sent are written to the specified file.
Each entry is JSON and contains the full message plus the Kafka error number and reason.

Action usage
------------

.. _param-omkafka-action-errorfile:
.. _omkafka.parameter.action.errorfile:
.. code-block:: rsyslog

   action(type="omkafka" errorFile="/var/log/omkafka-error.json")

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

