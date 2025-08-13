.. _param-imjournal-statefile:
.. _imjournal.parameter.module.statefile:

.. meta::
   :tag: module:imjournal
   :tag: parameter:StateFile

StateFile
=========

.. index::
   single: imjournal; StateFile
   single: StateFile

.. summary-start

Specifies path to the state file holding the journal cursor.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: StateFile
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Defines where the module stores its persistent position in the journal. A relative
path is created inside rsyslog's working directory; an absolute path is used as
is.

Module usage
------------
.. _param-imjournal-module-statefile:
.. _imjournal.parameter.module.statefile-usage:
.. code-block:: rsyslog

   module(load="imjournal" StateFile="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. _imjournal.parameter.legacy.imjournalstatefile:

- $imjournalStateFile — maps to StateFile (status: legacy)

.. index::
   single: imjournal; $imjournalStateFile
   single: $imjournalStateFile

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
