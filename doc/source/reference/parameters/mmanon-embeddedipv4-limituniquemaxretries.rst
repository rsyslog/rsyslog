.. _param-mmanon-embeddedipv4-limituniquemaxretries:
.. _mmanon.parameter.input.embeddedipv4-limituniquemaxretries:

embeddedIpv4.limitUniqueMaxRetries
==================================

.. index::
   single: mmanon; embeddedIpv4.limitUniqueMaxRetries
   single: embeddedIpv4.limitUniqueMaxRetries

.. summary-start

Controls whether random-consistent-unique anonymization limits retries for
embedded IPv4 addresses.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: embeddedIpv4.limitUniqueMaxRetries
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 8.2410.0

Description
-----------
When enabled, ``random-consistent-unique`` mode will stop retrying after
:ref:`embeddedIpv4.uniqueRetryCount <param-mmanon-embeddedipv4-uniqueretrycount>`
attempts. After the retry limit is reached, the behavior is controlled by
:ref:`embeddedIpv4.maxRetryHandling <param-mmanon-embeddedipv4-maxretryhandling>`.

Input usage
-----------
.. _mmanon.parameter.input.embeddedipv4-limituniquemaxretries-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" embeddedIpv4.anonMode="random-consistent-unique"
          embeddedIpv4.limitUniqueMaxRetries="on")

See also
--------
:doc:`../../configuration/modules/mmanon`
