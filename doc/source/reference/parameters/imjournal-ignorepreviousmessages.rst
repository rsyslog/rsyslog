.. _param-imjournal-ignorepreviousmessages:
.. _imjournal.parameter.module.ignorepreviousmessages:

.. meta::
   :tag: module:imjournal
   :tag: parameter:IgnorePreviousMessages

IgnorePreviousMessages
======================

.. index::
   single: imjournal; IgnorePreviousMessages
   single: IgnorePreviousMessages

.. summary-start

Starts reading only new journal entries when no state file exists.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: IgnorePreviousMessages
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
If enabled and no state file is present, messages already stored in the journal
are skipped and only new entries are processed, preventing reprocessing of old
logs.

Module usage
------------
.. _param-imjournal-module-ignorepreviousmessages:
.. _imjournal.parameter.module.ignorepreviousmessages-usage:
.. code-block:: rsyslog

   module(load="imjournal" IgnorePreviousMessages="...")

Notes
-----
- Historic documentation called this a ``binary`` option; it is boolean.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. _imjournal.parameter.legacy.imjournalignorepreviousmessages:

- $ImjournalIgnorePreviousMessages — maps to IgnorePreviousMessages (status: legacy)

.. index::
   single: imjournal; $ImjournalIgnorePreviousMessages
   single: $ImjournalIgnorePreviousMessages

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
