.. _param-mmanon-ipv4-bits:
.. _mmanon.parameter.input.ipv4-bits:

ipv4.bits
=========

.. index::
   single: mmanon; ipv4.bits
   single: ipv4.bits

.. summary-start

Sets how many low-order bits of IPv4 addresses are anonymized.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv4.bits
:Scope: input
:Type: positive integer
:Default: input=16
:Required?: no
:Introduced: 7.3.7

Description
-----------
This sets the number of bits that should be anonymized (bits are from the right,
so lower bits are anonymized first). This setting permits to save network
information while still anonymizing user-specific data. The more bits you
discard, the better the anonymization obviously is. The default of 16 bits
reflects what German data privacy rules consider as being sufficiently
anonymized. We assume, this can also be used as a rough but conservative
guideline for other countries.

Note: when in :ref:`simple mode <param-mmanon-ipv4-mode>`, only bits
on a byte boundary can be specified. As such, any value other than 8,
16, 24 or 32 is invalid. If an invalid value is
given, it is rounded to the next byte boundary (so we favor stronger
anonymization in that case). For example, a bit value of 12 will become 16 in
simple mode (an error message is also emitted).

Input usage
-----------
.. _mmanon.parameter.input.ipv4-bits-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv4.mode="simple" ipv4.bits="24")

See also
--------
:doc:`../../configuration/modules/mmanon`
