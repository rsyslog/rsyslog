.. _param-imjournal-workaroundjournalbug:
.. _imjournal.parameter.module.workaroundjournalbug:

.. meta::
   :tag: module:imjournal
   :tag: parameter:WorkAroundJournalBug

WorkAroundJournalBug
====================

.. index::
   single: imjournal; WorkAroundJournalBug
   single: WorkAroundJournalBug

.. summary-start

Legacy flag with no current effect; retained for compatibility.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: WorkAroundJournalBug
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 8.37.0

Description
-----------
Originally enabled a workaround for a journald bug. The option has had no effect
since version 8.1910.0 and remains only for backward compatibility.

Module usage
------------
.. _param-imjournal-module-workaroundjournalbug:
.. _imjournal.parameter.module.workaroundjournalbug-usage:
.. code-block:: rsyslog

   module(load="imjournal" WorkAroundJournalBug="...")

Notes
-----
- Historic documentation called this a ``binary`` option; it is boolean.
- Deprecated; enabling or disabling changes nothing.

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
