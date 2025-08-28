.. _param-imjournal-ignorenonvalidstatefile:
.. _imjournal.parameter.module.ignorenonvalidstatefile:

.. meta::
   :tag: module:imjournal
   :tag: parameter:IgnoreNonValidStatefile

IgnoreNonValidStatefile
=======================

.. index::
   single: imjournal; IgnoreNonValidStatefile
   single: IgnoreNonValidStatefile

.. summary-start

Ignores corrupt state files and restarts reading from the beginning.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: IgnoreNonValidStatefile
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When a corrupted state file is encountered, the module discards it and continues
reading from the start of the journal (or from the end if ``IgnorePreviousMessages``
is enabled). A new valid state file is written after the next state persistence
or shutdown.

Module usage
------------
.. _param-imjournal-module-ignorenonvalidstatefile:
.. _imjournal.parameter.module.ignorenonvalidstatefile-usage:
.. code-block:: rsyslog

   module(load="imjournal" IgnoreNonValidStatefile="...")

Notes
-----
- Historic documentation called this a ``binary`` option; it is boolean.

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
