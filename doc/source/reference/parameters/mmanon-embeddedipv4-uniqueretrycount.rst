.. _param-mmanon-embeddedipv4-uniqueretrycount:
.. _mmanon.parameter.input.embeddedipv4-uniqueretrycount:

embeddedIpv4.uniqueRetryCount
=============================

.. index::
   single: mmanon; embeddedIpv4.uniqueRetryCount
   single: embeddedIpv4.uniqueRetryCount

.. summary-start

Sets the maximum retry count for embedded IPv4 random-consistent-unique
anonymization.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: embeddedIpv4.uniqueRetryCount
:Scope: input
:Type: integer
:Default: input=1000
:Required?: no
:Introduced: 8.2410.0

Description
-----------
Defines how many randomized replacements are attempted before giving up when
``embeddedIpv4.limitUniqueMaxRetries`` is enabled. The retry limit only applies
for ``random-consistent-unique`` mode.

Input usage
-----------
.. _mmanon.parameter.input.embeddedipv4-uniqueretrycount-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" embeddedIpv4.anonMode="random-consistent-unique"
          embeddedIpv4.limitUniqueMaxRetries="on" embeddedIpv4.uniqueRetryCount="500")

See also
--------
:doc:`../../configuration/modules/mmanon`
