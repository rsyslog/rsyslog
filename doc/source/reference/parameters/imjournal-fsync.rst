.. _param-imjournal-fsync:
.. _imjournal.parameter.module.fsync:

.. meta::
   :tag: module:imjournal
   :tag: parameter:FSync

FSync
=====

.. index::
   single: imjournal; FSync
   single: FSync

.. summary-start

Force ``fsync()`` on the state file to guard against crash corruption.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: FSync
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.1908.0

Description
-----------
When enabled, the module synchronizes the state file to persistent storage after
each write. This reduces the risk of duplicate messages after abrupt shutdowns
at the cost of additional I/O.

Module usage
------------
.. _param-imjournal-module-fsync:
.. _imjournal.parameter.module.fsync-usage:
.. code-block:: rsyslog

   module(load="imjournal" FSync="...")

Notes
-----
- Historic documentation called this a ``binary`` option; it is boolean.
- May significantly impact performance with low ``PersistStateInterval`` values.

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
