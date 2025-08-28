.. _param-imjournal-persiststateinterval:
.. _imjournal.parameter.module.persiststateinterval:

.. meta::
   :tag: module:imjournal
   :tag: parameter:PersistStateInterval

PersistStateInterval
====================

.. index::
   single: imjournal; PersistStateInterval
   single: PersistStateInterval

.. summary-start

Saves the journal cursor after every N messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: PersistStateInterval
:Scope: module
:Type: integer
:Default: module=10
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Defines how often the module writes its position in the journal to the state file.
The cursor is saved after each ``PersistStateInterval`` messages so rsyslog resumes
from the last processed entry on restart.

Module usage
------------
.. _param-imjournal-module-persiststateinterval:
.. _imjournal.parameter.module.persiststateinterval-usage:
.. code-block:: rsyslog

   module(load="imjournal" PersistStateInterval="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. _imjournal.parameter.legacy.imjournalpersiststateinterval:

- $imjournalPersistStateInterval — maps to PersistStateInterval (status: legacy)

.. index::
   single: imjournal; $imjournalPersistStateInterval
   single: $imjournalPersistStateInterval

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
