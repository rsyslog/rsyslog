.. _param-mmexternal-binary:
.. _mmexternal.parameter.input.binary:

binary
======

.. index::
   single: mmexternal; binary
   single: binary

.. summary-start

Specifies the external message modification plugin executable that
mmexternal invokes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmexternal`.

:Name: binary
:Scope: input
:Type: string
:Default: none
:Required?: yes
:Introduced: 8.3.0

Description
-----------
The name of the external message modification plugin to be called. This can
be a full path name. Command-line arguments for the executable can also be
included in the string.

Input usage
-----------
.. _mmexternal.parameter.input.binary-usage:

.. code-block:: rsyslog

   module(load="mmexternal")

   action(
       type="mmexternal"
       binary="/path/to/mmexternal.py"
   )

See also
--------
See also :doc:`../../configuration/modules/mmexternal`.
