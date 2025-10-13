.. _param-imklog-keepkerneltimestamp:
.. _imklog.parameter.module.keepkerneltimestamp:

KeepKernelTimestamp
===================

.. index::
   single: imklog; KeepKernelTimestamp
   single: KeepKernelTimestamp

.. summary-start

Keeps the kernel-supplied timestamp prefix in each message when kernel timestamps are parsed.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imklog`.

:Name: KeepKernelTimestamp
:Scope: module
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
If enabled, this option keeps the [timestamp] provided by the
kernel at the beginning of each message rather than to remove it, when it
could be parsed and converted into local time for use as regular message
time. Only used when ``ParseKernelTimestamp`` is on.

Notes
-----
- Legacy documentation referred to the type as "binary"; it behaves as a boolean value.

Module usage
------------
.. _param-imklog-module-keepkerneltimestamp:
.. _imklog.parameter.module.keepkerneltimestamp-usage:

.. code-block:: rsyslog

   module(load="imklog"
          parseKernelTimestamp="on"
          keepKernelTimestamp="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imklog.parameter.legacy.klogkeepkerneltimestamp:

- $klogKeepKernelTimestamp — maps to KeepKernelTimestamp (status: legacy)

.. index::
   single: imklog; $klogKeepKernelTimestamp
   single: $klogKeepKernelTimestamp

See also
--------
:doc:`../../configuration/modules/imklog`
