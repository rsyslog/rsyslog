.. _param-mmanon-embeddedipv4-maxretryhandling:
.. _mmanon.parameter.input.embeddedipv4-maxretryhandling:

embeddedIpv4.maxRetryHandling
=============================

.. index::
   single: mmanon; embeddedIpv4.maxRetryHandling
   single: embeddedIpv4.maxRetryHandling

.. summary-start

Selects how embedded IPv4 random-consistent-unique handles retry exhaustion.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: embeddedIpv4.maxRetryHandling
:Scope: input
:Type: string
:Default: input=accept-duplicates
:Required?: no
:Introduced: 8.2410.0

Description
-----------
Controls how ``random-consistent-unique`` behaves after the retry limit is
reached (see
:ref:`embeddedIpv4.limitUniqueMaxRetries <param-mmanon-embeddedipv4-limituniquemaxretries>`).
The supported values are:

- ``zero``: fall back to zeroing the anonymized bits.
- ``accept-duplicates``: keep the last randomized address even if it already
  exists.

A warning is logged whenever the retry limit is hit.

Input usage
-----------
.. _mmanon.parameter.input.embeddedipv4-maxretryhandling-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" embeddedIpv4.anonMode="random-consistent-unique"
          embeddedIpv4.limitUniqueMaxRetries="on"
          embeddedIpv4.maxRetryHandling="accept-duplicates")

See also
--------
:doc:`../../configuration/modules/mmanon`
