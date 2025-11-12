.. _param-imklog-permitnonkernelfacility:
.. _imklog.parameter.module.permitnonkernelfacility:

PermitNonKernelFacility
=======================

.. index::
   single: imklog; PermitNonKernelFacility
   single: PermitNonKernelFacility

.. summary-start

Controls whether imklog submits kernel log messages that use non-kernel facilities.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imklog`.

:Name: PermitNonKernelFacility
:Scope: module
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
At least under BSD the kernel log may contain entries with non-kernel
facilities. This setting controls how those are handled. The default is
"off", in which case these messages are ignored. Switch it to on to
submit non-kernel messages to rsyslog processing.

Notes
-----
- Legacy documentation referred to the type as "binary"; it behaves as a boolean value.

Module usage
------------
.. _param-imklog-module-permitnonkernelfacility:
.. _imklog.parameter.module.permitnonkernelfacility-usage:

.. code-block:: rsyslog

   module(load="imklog" permitNonKernelFacility="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imklog.parameter.legacy.klogpermitnonkernelfacility:

- $KLogPermitNonKernelFacility — maps to PermitNonKernelFacility (status: legacy)

.. index::
   single: imklog; $KLogPermitNonKernelFacility
   single: $KLogPermitNonKernelFacility

See also
--------
:doc:`../../configuration/modules/imklog`
