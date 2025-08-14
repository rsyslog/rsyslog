.. _param-imudp-ipfreebind:
.. _imudp.parameter.module.ipfreebind:

IpFreeBind
==========

.. index::
   single: imudp; IpFreeBind
   single: IpFreeBind

.. summary-start

Controls the `IP_FREEBIND` socket option for binding to nonlocal addresses.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: IpFreeBind
:Scope: input
:Type: integer
:Default: input=2
:Required?: no
:Introduced: 8.18.0

Description
-----------
Determines how the module handles nonlocal bind addresses: `0` disables, `1` enables silently, and `2` enables with a warning.

Input usage
-----------
.. _param-imudp-input-ipfreebind:
.. _imudp.parameter.input.ipfreebind:
.. code-block:: rsyslog

   input(type="imudp" IpFreeBind="...")

Notes
-----
.. versionadded:: 8.18.0

See also
--------
See also :doc:`../../configuration/modules/imudp`.
