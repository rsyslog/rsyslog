.. _param-mmanon-ipv6-limituniquemaxretries:
.. _mmanon.parameter.input.ipv6-limituniquemaxretries:

ipv6.limitUniqueMaxRetries
==========================

.. index::
   single: mmanon; ipv6.limitUniqueMaxRetries
   single: ipv6.limitUniqueMaxRetries

.. summary-start

Controls whether random-consistent-unique anonymization limits retries for IPv6.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv6.limitUniqueMaxRetries
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 8.2410.0

Description
-----------
When enabled, ``random-consistent-unique`` mode will stop retrying after
:ref:`ipv6.uniqueRetryCount <param-mmanon-ipv6-uniqueretrycount>` attempts.
After the retry limit is reached, the behavior is controlled by
:ref:`ipv6.maxRetryHandling <param-mmanon-ipv6-maxretryhandling>`.

Input usage
-----------
.. _mmanon.parameter.input.ipv6-limituniquemaxretries-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv6.anonMode="random-consistent-unique"
          ipv6.limitUniqueMaxRetries="on")

See also
--------
:doc:`../../configuration/modules/mmanon`
