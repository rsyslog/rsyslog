.. SPDX-License-Identifier: Apache-2.0

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

The same applies when the state file path exists but is not a regular file,
for example a directory, or if ``fstat()`` on it fails. imjournal does not remove or
replace such a path, so persisting the state keeps failing (and logging an
error) until the administrator removes it. If this parameter is ``off``, any
of these conditions stops the imjournal input.

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
