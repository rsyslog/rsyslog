.. _param-mmanon-ipv6-uniqueretrycount:
.. _mmanon.parameter.input.ipv6-uniqueretrycount:

ipv6.uniqueRetryCount
=====================

.. index::
   single: mmanon; ipv6.uniqueRetryCount
   single: ipv6.uniqueRetryCount

.. summary-start

Sets the maximum retry count for IPv6 random-consistent-unique anonymization.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv6.uniqueRetryCount
:Scope: input
:Type: integer
:Default: input=1000
:Required?: no
:Introduced: 8.2410.0

Description
-----------
Defines how many randomized replacements are attempted before giving up when
``ipv6.limitUniqueMaxRetries`` is enabled. The retry limit only applies to
``random-consistent-unique`` mode.

Input usage
-----------
.. _mmanon.parameter.input.ipv6-uniqueretrycount-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv6.anonMode="random-consistent-unique"
          ipv6.limitUniqueMaxRetries="on" ipv6.uniqueRetryCount="500")

See also
--------
:doc:`../../configuration/modules/mmanon`
