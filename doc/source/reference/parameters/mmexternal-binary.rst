.. _param-mmexternal-binary:
.. _mmexternal.parameter.module.binary:

Binary
======

.. index::
   single: mmexternal; Binary
   single: Binary

.. summary-start

Specifies the external message modification plugin executable that mmexternal invokes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmexternal`.

:Name: Binary
:Scope: module
:Type: string
:Default: module=none
:Required?: yes
:Introduced: 8.3.0

Description
-----------
The name of the external message modification plugin to be called. This can be a full path name.

Module usage
------------
.. _param-mmexternal-module-binary:
.. _mmexternal.parameter.module.binary-usage:

.. code-block:: rsyslog

   module(load="mmexternal")

   action(
       type="mmexternal"
       binary="/path/to/mmexternal.py"
   )

See also
--------
See also :doc:`../../configuration/modules/mmexternal`.
