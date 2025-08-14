.. _param-imudp-defaulttz:
.. _imudp.parameter.module.defaulttz:

DefaultTZ
=========

.. index::
   single: imudp; DefaultTZ
   single: DefaultTZ

.. summary-start

Applies a default timezone offset to legacy messages lacking timezone data.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: DefaultTZ
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Sets a fixed timezone offset like `+01:30` for messages without explicit timezone information; intended for experimental use.

Input usage
-----------
.. _param-imudp-input-defaulttz:
.. _imudp.parameter.input.defaulttz:
.. code-block:: rsyslog

   input(type="imudp" DefaultTZ="...")

Notes
-----
This is an experimental parameter and may change or be removed without notice.

See also
--------
See also :doc:`../../configuration/modules/imudp`.
