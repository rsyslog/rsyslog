.. _param-mmanon-ipv4-replacechar:
.. _mmanon.parameter.module.ipv4-replacechar:

ipv4.replaceChar
================

.. index::
   single: mmanon; ipv4.replaceChar
   single: ipv4.replaceChar

.. summary-start

Sets the character used to overwrite anonymized IPv4 octets in simple mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv4.replaceChar
:Scope: module
:Type: character
:Default: module=x
:Required?: no
:Introduced: 7.3.7

Description
-----------
In simple mode, this sets the character that the to-be-anonymized part of the IP address is to be overwritten with. In any other mode the parameter is ignored if set.

Module usage
------------
.. _param-mmanon-module-ipv4-replacechar:
.. _mmanon.parameter.module.ipv4-replacechar-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv4.mode="simple" ipv4.replaceChar="*")

See also
--------
See also :doc:`../../configuration/modules/mmanon`.
