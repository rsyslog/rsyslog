.. _param-mmanon-embeddedipv4-anonmode:
.. _mmanon.parameter.input.embeddedipv4-anonmode:

embeddedIpv4.anonMode
=====================

.. index::
   single: mmanon; embeddedIpv4.anonMode
   single: embeddedIpv4.anonMode

.. summary-start

Defines how IPv6 addresses with embedded IPv4 parts are anonymized.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: embeddedIpv4.anonMode
:Scope: input
:Type: string
:Default: input=zero
:Required?: no
:Introduced: 7.3.7

Description
-----------
The available modes are ``random``, ``random-consistent``,
``random-consistent-unique``, and ``zero``.

The modes ``random`` and ``random-consistent`` are very similar, in that they
both anonymize IP addresses by randomizing the last bits (any number) of a given
address. However, while ``random`` mode assigns a new random IP address for
every address in a message, ``random-consistent`` will assign the same
randomized address to every instance of the same original address.
``random-consistent-unique`` builds on that behavior and guarantees that no
two distinct original addresses share the same anonymized result. It may
take longer to generate addresses because a replacement is retried until an
unused randomized value is found; this effect grows as more unique IPs are
processed.

The default ``zero`` mode will do full anonymization of any number of bits and
it will also normalize the address, so that no information about the original IP
address is available.

Also note that an anonymized IPv6 address will be normalized, meaning there will
be no abbreviations, leading zeros will **not** be displayed, and capital
letters in the hex numerals will be lowercase.

Input usage
-----------
.. _mmanon.parameter.input.embeddedipv4-anonmode-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" embeddedIpv4.anonMode="random")

See also
--------
:doc:`../../configuration/modules/mmanon`
