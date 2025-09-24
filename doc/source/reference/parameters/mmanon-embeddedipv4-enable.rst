.. _param-mmanon-embeddedipv4-enable:
.. _mmanon.parameter.input.embeddedipv4-enable:

embeddedIPv4.enable
===================

.. index::
   single: mmanon; embeddedIPv4.enable
   single: embeddedIPv4.enable

.. summary-start

Enables or disables anonymization of IPv6 addresses with embedded IPv4 parts.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: embeddedIPv4.enable
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: 7.3.7

Description
-----------
Allows to enable or disable the anonymization of IPv6 addresses with embedded
IPv4.

Input usage
-----------
.. _mmanon.parameter.input.embeddedipv4-enable-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" embeddedIPv4.enable="off")

See also
--------
:doc:`../../configuration/modules/mmanon`
