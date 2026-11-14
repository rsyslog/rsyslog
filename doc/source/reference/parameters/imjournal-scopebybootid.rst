.. _param-imjournal-scopebybootid:
.. _imjournal.parameter.module.scopebybootid:

.. meta::
   :tag: module:imjournal
   :tag: parameter:ScopeByBootId

ScopeByBootId
=============

.. index::
   single: imjournal; ScopeByBootId
   single: ScopeByBootId

.. summary-start

Read the journal one boot at a time and start with the current boot when no
saved position exists.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: ScopeByBootId
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.2610.0

Description
-----------
When enabled, imjournal restricts the journal to the entries of a single boot
(``_BOOT_ID``) at a time.

- If a saved position from the state file exists, imjournal first reads the
  remaining entries of the boot the saved position belongs to. Afterwards it
  continues with the first entry of each following boot until it reaches the
  current boot.
- If no state file exists, or it is ignored because it is invalid (see
  :ref:`param-imjournal-ignorenonvalidstatefile`), imjournal starts with the
  first entry of the current boot instead of the oldest entry in the journal.

Without this option the saved position is located across all boots. If the
journal file that holds the saved entry has changed, journald falls back to
the wall clock time of the entry to find the position. On systems whose clock
is only set some time after boot, e.g. systems without an RTC that rely on
NTP, the entries logged early during the following boot carry a wall clock
time that is older than the saved entry and are skipped. Reading each boot
from its first entry avoids this.

The boot that follows an earlier boot is the boot of the first entry after the
last processed entry that does not belong to a boot that was already read
completely.

This option cannot be combined with :ref:`param-imjournal-remote`, because
entries of remote machines carry boot ids of those machines. It has no effect
while :ref:`param-imjournal-ignorepreviousmessages` skips older entries.

Module usage
------------
.. _param-imjournal-module-scopebybootid:
.. _imjournal.parameter.module.scopebybootid-usage:
.. code-block:: rsyslog

   module(load="imjournal" StateFile="imjournal.state" ScopeByBootId="on")

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
