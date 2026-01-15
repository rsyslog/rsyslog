.. _param-mmanon-ipv4-maxretryhandling:
.. _mmanon.parameter.input.ipv4-maxretryhandling:

ipv4.maxRetryHandling
=====================

.. index::
   single: mmanon; ipv4.maxRetryHandling
   single: ipv4.maxRetryHandling

.. summary-start

Selects how IPv4 random-consistent-unique handles retry exhaustion.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv4.maxRetryHandling
:Scope: input
:Type: string
:Default: input=accept-duplicates
:Required?: no
:Introduced: 8.2410.0

Description
-----------
Controls how ``random-consistent-unique`` behaves after the retry limit is
reached (see :ref:`ipv4.limitUniqueMaxRetries <param-mmanon-ipv4-limituniquemaxretries>`).
The supported values are:

- ``zero``: fall back to zeroing the anonymized bits.
- ``accept-duplicates``: keep the last randomized address even if it already
  exists.

A warning is logged whenever the retry limit is hit.

Input usage
-----------
.. _mmanon.parameter.input.ipv4-maxretryhandling-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv4.mode="random-consistent-unique"
          ipv4.limitUniqueMaxRetries="on" ipv4.maxRetryHandling="zero")

See also
--------
:doc:`../../configuration/modules/mmanon`
