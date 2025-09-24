.. _param-mmanon-ipv4-mode:
.. _mmanon.parameter.input.ipv4-mode:

ipv4.mode
=========

.. index::
   single: mmanon; ipv4.mode
   single: ipv4.mode

.. summary-start

Selects the IPv4 anonymization mode used by the mmanon action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv4.mode
:Scope: input
:Type: string
:Default: input=zero
:Required?: no
:Introduced: 7.3.7

Description
-----------
The available modes are "simple", "random", "random-consistent", and "zero". In
simple mode, only octets as a whole can be anonymized and the length of the
message is never changed. This means that when the last three octets of the
address 10.1.12.123 are anonymized, the result will be 10.0.00.000. This means
that the length of the original octets is still visible and may be used to draw
some privacy-evasive conclusions. This mode is slightly faster than the other
modes, and this may matter in high throughput environments.

The modes "random" and "random-consistent" are very similar, in that they both
anonymize IP addresses by randomizing the last bits (any number) of a given
address. However, while "random" mode assigns a new random IP address for every
address in a message, "random-consistent" will assign the same randomized
address to every instance of the same original address.

The default "zero" mode will do full anonymization of any number of bits. It
will also normalize the address, so that no information about the original IP
address is available. So in the above example, 10.1.12.123 would be anonymized
to 10.0.0.0.

Input usage
-----------
.. _mmanon.parameter.input.ipv4-mode-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv4.mode="random-consistent")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _mmanon.parameter.legacy.mode:

- mode — maps to ipv4.mode (status: legacy)

.. index::
   single: mmanon; mode
   single: mode

See also
--------
See also :doc:`../../configuration/modules/mmanon`.
