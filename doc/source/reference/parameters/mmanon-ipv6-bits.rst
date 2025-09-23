.. _param-mmanon-ipv6-bits:
.. _mmanon.parameter.module.ipv6-bits:

ipv6.bits
=========

.. index::
   single: mmanon; ipv6.bits
   single: ipv6.bits

.. summary-start

Sets how many low-order bits of IPv6 addresses are anonymized.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv6.bits
:Scope: module
:Type: positive integer
:Default: module=96
:Required?: no
:Introduced: 7.3.7

Description
-----------
This sets the number of bits that should be anonymized (bits are from the right, so lower bits are anonymized first). This setting permits to save network information while still anonymizing user-specific data. The more bits you discard, the better the anonymization obviously is. The default of 96 bits reflects what German data privacy rules consider as being sufficiently anonymized. We assume, this can also be used as a rough but conservative guideline for other countries.

Module usage
------------
.. _param-mmanon-module-ipv6-bits:
.. _mmanon.parameter.module.ipv6-bits-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv6.bits="64")

See also
--------
See also :doc:`../../configuration/modules/mmanon`.
