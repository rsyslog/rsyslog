.. _param-imfile-persiststateinterval:
.. _imfile.parameter.input.persiststateinterval:
.. _imfile.parameter.persiststateinterval:

PersistStateInterval
====================

.. index::
   single: imfile; PersistStateInterval
   single: PersistStateInterval

.. summary-start

Controls how often the state file is written based on processed line count.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: PersistStateInterval
:Scope: input
:Type: integer
:Default: 0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies how often the state file is written when processing the input
file. A value of ``0`` means the state file is written when the monitored
file is closed, typically at rsyslog shutdown. Any other value ``n``
causes the state file to be written after at least every ``n`` processed
lines. This setting can guard against message duplication due to fatal
errors but may reduce performance if set to a low value. Rsyslog may write
state files more frequently if needed.

**Note: If this parameter is not set, state files are not created.**

Input usage
-----------
.. _param-imfile-input-persiststateinterval:
.. _imfile.parameter.input.persiststateinterval-usage:

.. code-block:: rsyslog

   input(type="imfile" PersistStateInterval="0")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilepersiststateinterval:
- ``$InputFilePersistStateInterval`` — maps to PersistStateInterval (status: legacy)

.. index::
   single: imfile; $InputFilePersistStateInterval
   single: $InputFilePersistStateInterval

See also
--------
See also :doc:`../../configuration/modules/imfile`.
