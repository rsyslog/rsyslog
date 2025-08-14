.. _param-imfile-persiststateinterval:
.. _imfile.parameter.module.persiststateinterval:

PersistStateInterval
====================

.. index::
   single: imfile; PersistStateInterval
   single: PersistStateInterval

.. summary-start

Specifies how often the state file shall be written when processing the input file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: PersistStateInterval
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies how often the state file shall be written when processing
the input file. The **default** value is 0, which means a new state
file is at least being written when the monitored files is being closed (end of
rsyslogd execution). Any other value n means that the state file is
written at least every time n file lines have been processed. This setting can
be used to guard against message duplication due to fatal errors
(like power fail). Note that this setting affects imfile performance,
especially when set to a low value. Frequently writing the state file
is very time consuming.

Note further that rsyslog may write state files
more frequently. This happens if rsyslog has some reason to do so.
There is intentionally no more precise description of when state files
are being written, as this is an implementation detail and may change
as needed.

**Note: If this parameter is not set, state files are not created.**

Input usage
-----------
.. _param-imfile-input-persiststateinterval:
.. _imfile.parameter.input.persiststateinterval:
.. code-block:: rsyslog

   input(type="imfile" PersistStateInterval="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilepersiststateinterval:
- $InputFilePersistStateInterval — maps to PersistStateInterval (status: legacy)

.. index::
   single: imfile; $InputFilePersistStateInterval
   single: $InputFilePersistStateInterval

See also
--------
See also :doc:`../../configuration/modules/imfile`.
