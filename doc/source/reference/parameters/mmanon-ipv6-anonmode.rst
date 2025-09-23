.. _param-mmanon-ipv6-anonmode:
.. _mmanon.parameter.module.ipv6-anonmode:

ipv6.anonmode
=============

.. index::
   single: mmanon; ipv6.anonmode
   single: ipv6.anonmode

.. summary-start

Defines how IPv6 addresses are anonymized by the mmanon action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv6.anonmode
:Scope: module
:Type: string
:Default: module=zero
:Required?: no
:Introduced: 7.3.7

Description
-----------
This defines the mode, in which IPv6 addresses will be anonymized. There exist the "random", "random-consistent", and "zero" modes.

The modes "random" and "random-consistent" are very similar, in that they both anonymize IP addresses by randomizing the last bits (any number) of a given address. However, while "random" mode assigns a new random IP address for every address in a message, "random-consistent" will assign the same randomized address to every instance of the same original address.

The default "zero" mode will do full anonymization of any number of bits and it will also normalize the address, so that no information about the original IP address is available.

Also note that an anonymized IPv6 address will be normalized, meaning there will be no abbreviations, leading zeros will **not** be displayed, and capital letters in the hex numerals will be lowercase.

Module usage
------------
.. _param-mmanon-module-ipv6-anonmode:
.. _mmanon.parameter.module.ipv6-anonmode-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv6.anonmode="random-consistent")

See also
--------
See also :doc:`../../configuration/modules/mmanon`.
