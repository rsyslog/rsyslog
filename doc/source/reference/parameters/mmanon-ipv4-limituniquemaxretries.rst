.. _param-mmanon-ipv4-limituniquemaxretries:
.. _mmanon.parameter.input.ipv4-limituniquemaxretries:

ipv4.limitUniqueMaxRetries
==========================

.. index::
   single: mmanon; ipv4.limitUniqueMaxRetries
   single: ipv4.limitUniqueMaxRetries

.. summary-start

Controls whether random-consistent-unique anonymization limits retries for IPv4.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv4.limitUniqueMaxRetries
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 8.2410.0

Description
-----------
When enabled, ``random-consistent-unique`` mode will stop retrying after
:ref:`ipv4.uniqueRetryCount <param-mmanon-ipv4-uniqueretrycount>` attempts.
After the retry limit is reached, the behavior is controlled by
:ref:`ipv4.maxRetryHandling <param-mmanon-ipv4-maxretryhandling>`.

Input usage
-----------
.. _mmanon.parameter.input.ipv4-limituniquemaxretries-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv4.mode="random-consistent-unique"
          ipv4.limitUniqueMaxRetries="on")

See also
--------
:doc:`../../configuration/modules/mmanon`
