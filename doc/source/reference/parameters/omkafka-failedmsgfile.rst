.. _param-omkafka-failedmsgfile:
.. _omkafka.parameter.module.failedmsgfile:

failedMsgFile
=============

.. index::
   single: omkafka; failedMsgFile
   single: failedMsgFile

.. summary-start

File that stores messages saved by `KeepFailedMessages`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkafka`.

:Name: failedMsgFile
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: 8.28.0

Description
-----------

.. versionadded:: 8.28.0

Filename where the failed messages should be stored. Must be set when
``KeepFailedMessages`` is enabled.

Action usage
------------

.. _param-omkafka-action-failedmsgfile:
.. _omkafka.parameter.action.failedmsgfile:
.. code-block:: rsyslog

   action(type="omkafka" failedMsgFile="/var/spool/rsyslog/failed.kafkamsgs")

See also
--------

See also :doc:`../../configuration/modules/omkafka`.

