.. _param-mmanon-ipv4-uniqueretrycount:
.. _mmanon.parameter.input.ipv4-uniqueretrycount:

ipv4.uniqueRetryCount
=====================

.. index::
   single: mmanon; ipv4.uniqueRetryCount
   single: ipv4.uniqueRetryCount

.. summary-start

Sets the maximum retry count for IPv4 random-consistent-unique anonymization.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmanon`.

:Name: ipv4.uniqueRetryCount
:Scope: input
:Type: integer
:Default: input=1000
:Required?: no
:Introduced: 8.2410.0

Description
-----------
Defines how many randomized replacements are attempted before giving up when
``ipv4.limitUniqueMaxRetries`` is enabled. The retry limit only applies to
``random-consistent-unique`` mode.

Input usage
-----------
.. _mmanon.parameter.input.ipv4-uniqueretrycount-usage:

.. code-block:: rsyslog

   module(load="mmanon")
   action(type="mmanon" ipv4.mode="random-consistent-unique"
          ipv4.limitUniqueMaxRetries="on" ipv4.uniqueRetryCount="500")

See also
--------
:doc:`../../configuration/modules/mmanon`
