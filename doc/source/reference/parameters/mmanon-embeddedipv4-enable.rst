.. _param-mmanon-embeddedipv4-enable:
.. _mmanon.parameter.input.embeddedipv4-enable:

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
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: 7.3.7

Description
-----------
This parameter controls whether ``mmanon`` will attempt to find and anonymize
IPv6 addresses with embedded IPv4 parts. If set to ``off``, all other
``embeddedipv4.*`` parameters for this action are ignored.

Input usage
-----------
.. _mmanon.parameter.input.embeddedipv4-enable-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" embeddedipv4.enable="off")

See also
--------
:doc:`../../configuration/modules/mmanon`
