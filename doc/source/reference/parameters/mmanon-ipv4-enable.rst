.. _param-mmanon-ipv4-enable:
.. _mmanon.parameter.module.ipv4-enable:

ipv4.enable
===========

.. index::
   single: mmanon; ipv4.enable
   single: ipv4.enable

.. summary-start

Enables or disables IPv4 address anonymization for the mmanon action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv4.enable
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 7.3.7

Description
-----------
Allows to enable or disable the anonymization of IPv4 addresses.

Module usage
------------
.. _param-mmanon-module-ipv4-enable:
.. _mmanon.parameter.module.ipv4-enable-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv4.enable="off")

See also
--------
See also :doc:`../../configuration/modules/mmanon`.
