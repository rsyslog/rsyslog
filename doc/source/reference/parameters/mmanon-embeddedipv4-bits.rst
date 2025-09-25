.. _param-mmanon-embeddedipv4-bits:
.. _mmanon.parameter.input.embeddedipv4-bits:

embeddedipv4.bits
=================

.. index::
   single: mmanon; embeddedipv4.bits
   single: embeddedipv4.bits

.. summary-start

Sets how many low-order bits of embedded IPv4 addresses within IPv6 are
anonymized.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: embeddedipv4.bits
:Scope: input
:Type: positive integer
:Default: input=96
:Required?: no
:Introduced: 7.3.7

Description
-----------
This sets the number of bits that should be anonymized (bits are from the right,
so lower bits are anonymized first). This setting permits to save network
information while still anonymizing user-specific data. The more bits you
discard, the better the anonymization obviously is. The default of 96 bits
reflects what German data privacy rules consider as being sufficiently
anonymized. We assume, this can also be used as a rough but conservative
guideline for other countries.

Input usage
-----------
.. _mmanon.parameter.input.embeddedipv4-bits-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" embeddedipv4.bits="80")

See also
--------
:doc:`../../configuration/modules/mmanon`
