.. _param-imudp-preservecase:
.. _imudp.parameter.module.preservecase:

PreserveCase
============

.. index::
   single: imudp; PreserveCase
   single: PreserveCase

.. summary-start

Controls whether ``fromhost`` preserves original case.

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
This parameter is for controlling the case in ``fromhost``. If
``PreserveCase`` is set to "on", the case in ``fromhost`` is preserved, e.g.,
``Host1.Example.Org`` when the message was received from
``Host1.Example.Org``. Default is "off" for backward compatibility.

Module usage
------------
.. _param-imudp-module-preservecase:
.. _imudp.parameter.module.preservecase-usage:

.. code-block:: rsyslog

   module(load="imudp" PreserveCase="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
