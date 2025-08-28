.. _param-imjournal-defaultseverity:
.. _imjournal.parameter.module.defaultseverity:

.. meta::
   :tag: module:imjournal
   :tag: parameter:DefaultSeverity

DefaultSeverity
===============

.. index::
   single: imjournal; DefaultSeverity
   single: DefaultSeverity

.. summary-start

Fallback severity used if a journal entry lacks ``SYSLOG_PRIORITY``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: DefaultSeverity
:Scope: module
:Type: word
:Default: module=notice
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the severity assigned to messages that do not provide the
``SYSLOG_PRIORITY`` field. Values may be given as names or numbers.

Module usage
------------
.. _param-imjournal-module-defaultseverity:
.. _imjournal.parameter.module.defaultseverity-usage:
.. code-block:: rsyslog

   module(load="imjournal" DefaultSeverity="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. _imjournal.parameter.legacy.imjournaldefaultseverity:

- $ImjournalDefaultSeverity — maps to DefaultSeverity (status: legacy)

.. index::
   single: imjournal; $ImjournalDefaultSeverity
   single: $ImjournalDefaultSeverity

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
