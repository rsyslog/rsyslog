.. _param-omfile-dynafile-donotsuspend:
.. _omfile.parameter.module.dynafile-donotsuspend:

dynafile.donotsuspend
=====================

.. index::
   single: omfile; dynafile.donotsuspend
   single: dynafile.donotsuspend

.. summary-start

This permits SUSPENDing dynafile actions.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: dynafile.donotsuspend
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

This permits SUSPENDing dynafile actions. Traditionally, SUSPEND mode was
never entered for dynafiles as it would have blocked overall processing
flow. Default is not to suspend (and thus block).

Module usage
------------

.. _param-omfile-module-dynafile-donotsuspend:
.. _omfile.parameter.module.dynafile-donotsuspend-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" dynafile.donotsuspend="...")

Notes
-----

- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

See also
--------

See also :doc:`../../configuration/modules/omfile`.
