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

If the state file path exists but is not a regular file (for example a
directory, a FIFO or a device), or if ``fstat()`` on it fails, the module also
ignores it and reads from the start of the journal (or from the end if
``IgnorePreviousMessages`` is enabled). Unlike a corrupt file, a path that is
not a regular file is not replaced: imjournal leaves it untouched and does not
save its position while the path is there. This is logged once; once the path
is removed (or replaced by a regular file), saving resumes at the next state
persistence or shutdown. If this parameter is ``off``, any of these conditions
stops the imjournal input.

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
