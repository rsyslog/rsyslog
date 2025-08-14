.. _param-imudp-preservecase:
.. _imudp.parameter.module.preservecase:

PreserveCase
============

.. index::
   single: imudp; PreserveCase
   single: PreserveCase

.. summary-start

Preserves the original casing of the `fromhost` property when set to `on`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: PreserveCase
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.37.0

Description
-----------
Keeps the case of the remote host name as received instead of converting it, aiding differentiation of hosts with case-sensitive identifiers.

Module usage
------------
.. _param-imudp-module-preservecase:
.. _imudp.parameter.module.preservecase-usage:
.. code-block:: rsyslog

   module(load="imudp" PreserveCase="...")

Notes
-----
.. versionadded:: 8.37.0

See also
--------
See also :doc:`../../configuration/modules/imudp`.
