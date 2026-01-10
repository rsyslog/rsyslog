.. _param-mmanon-ipv6-maxretryhandling:
.. _mmanon.parameter.input.ipv6-maxretryhandling:

ipv6.maxRetryHandling
=====================

.. index::
   single: mmanon; ipv6.maxRetryHandling
   single: ipv6.maxRetryHandling

.. summary-start

Selects how IPv6 random-consistent-unique handles retry exhaustion.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv6.maxRetryHandling
:Scope: input
:Type: string
:Default: input=accept-duplicates
:Required?: no
:Introduced: 8.2410.0

Description
-----------
Controls how ``random-consistent-unique`` behaves after the retry limit is
reached (see :ref:`ipv6.limitUniqueMaxRetries <param-mmanon-ipv6-limituniquemaxretries>`).
The supported values are:

- ``zero``: fall back to zeroing the anonymized bits.
- ``accept-duplicates``: keep the last randomized address even if it already
  exists.

A warning is logged whenever the retry limit is hit.

Input usage
-----------
.. _mmanon.parameter.input.ipv6-maxretryhandling-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv6.anonMode="random-consistent-unique"
          ipv6.limitUniqueMaxRetries="on" ipv6.maxRetryHandling="accept-duplicates")

See also
--------
:doc:`../../configuration/modules/mmanon`
