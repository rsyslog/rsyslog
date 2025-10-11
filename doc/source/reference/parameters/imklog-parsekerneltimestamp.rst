.. _param-imklog-parsekerneltimestamp:
.. _imklog.parameter.module.parsekerneltimestamp:

ParseKernelTimestamp
====================

.. index::
   single: imklog; ParseKernelTimestamp
   single: ParseKernelTimestamp

.. summary-start

Parses kernel-provided timestamps and uses them as the message time instead of the receive time.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imklog`.

:Name: ParseKernelTimestamp
:Scope: module
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
If enabled and the kernel creates a timestamp for its log messages, this
timestamp will be parsed and converted into regular message time instead
of using the receive time of the kernel message (as in 5.8.x and before).
Default is 'off' to prevent parsing the kernel timestamp, because the
clock used by the kernel to create the timestamps is not supposed to be
as accurate as the monotonic clock required to convert it. Depending on
the hardware and kernel, it can result in message time differences
between kernel and system messages which occurred at the same time.

Notes
-----
- Legacy documentation referred to the type as "binary"; it behaves as a boolean value.

Module usage
------------
.. _param-imklog-module-parsekerneltimestamp:
.. _imklog.parameter.module.parsekerneltimestamp-usage:

.. code-block:: rsyslog

   module(load="imklog" parseKernelTimestamp="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imklog.parameter.legacy.klogparsekerneltimestamp:

- $klogParseKernelTimestamp — maps to ParseKernelTimestamp (status: legacy)

.. index::
   single: imklog; $klogParseKernelTimestamp
   single: $klogParseKernelTimestamp

See also
--------
:doc:`../../configuration/modules/imklog`
