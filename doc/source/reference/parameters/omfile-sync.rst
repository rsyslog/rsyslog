.. _param-omfile-sync:
.. _omfile.parameter.module.sync:

sync
====

.. index::
   single: omfile; sync
   single: sync

.. summary-start

Enables file syncing capability of omfile.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: sync
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Enables file syncing capability of omfile.

When enabled, rsyslog does a sync to the data file as well as the
directory it resides after processing each batch. There currently
is no way to sync only after each n-th batch.

Enabling sync causes a severe performance hit. Actually,
it slows omfile so much down, that the probability of losing messages
**increases**. In short,
you should enable syncing only if you know exactly what you do, and
fully understand how the rest of the engine works, and have tuned
the rest of the engine to lossless operations.

Action usage
------------

.. _param-omfile-action-sync:
.. _omfile.parameter.action.sync:
.. code-block:: rsyslog

   action(type="omfile" sync="...")

Notes
-----

- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.actionfileenablesync:

- $ActionFileEnableSync — maps to sync (status: legacy)

.. index::
   single: omfile; $ActionFileEnableSync
   single: $ActionFileEnableSync

See also
--------

See also :doc:`../../configuration/modules/omfile`.
