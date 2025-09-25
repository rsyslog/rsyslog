.. _param-mmanon-ipv6-enable:
.. _mmanon.parameter.input.ipv6-enable:

ipv6.enable
===========

.. index::
   single: mmanon; ipv6.enable
   single: ipv6.enable

.. summary-start

Enables or disables IPv6 address anonymization for the mmanon action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv6.enable
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: 7.3.7

Description
-----------
This parameter controls whether ``mmanon`` will attempt to find and anonymize
IPv6 addresses. If set to ``off``, all other ``ipv6.*`` parameters for this
action are ignored. Note that this does not affect IPv6 addresses with embedded
IPv4 parts, which are controlled by ``embeddedIpv4.enable``.

Input usage
-----------
.. _mmanon.parameter.input.ipv6-enable-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv6.enable="off")

See also
--------
:doc:`../../configuration/modules/mmanon`
