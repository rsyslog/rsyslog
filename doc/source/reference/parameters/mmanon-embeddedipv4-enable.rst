.. _param-mmanon-embeddedipv4-enable:
.. _mmanon.parameter.module.embeddedipv4-enable:

embeddedipv4.enable
===================

.. index::
   single: mmanon; embeddedipv4.enable
   single: embeddedipv4.enable

.. summary-start

Enables or disables anonymization of IPv6 addresses with embedded IPv4 parts.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: embeddedipv4.enable
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 7.3.7

Description
-----------
Allows to enable or disable the anonymization of IPv6 addresses with embedded IPv4.

Module usage
------------
.. _param-mmanon-module-embeddedipv4-enable:
.. _mmanon.parameter.module.embeddedipv4-enable-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" embeddedipv4.enable="off")

See also
--------
See also :doc:`../../configuration/modules/mmanon`.
