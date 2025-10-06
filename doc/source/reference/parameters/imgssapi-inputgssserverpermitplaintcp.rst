.. _param-imgssapi-inputgssserverpermitplaintcp:
.. _imgssapi.parameter.input.inputgssserverpermitplaintcp:

InputGSSServerPermitPlainTCP
============================

.. index::
   single: imgssapi; InputGSSServerPermitPlainTCP
   single: InputGSSServerPermitPlainTCP

.. summary-start

Allows accepting plain TCP syslog traffic on the GSS-protected port.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imgssapi`.

:Name: InputGSSServerPermitPlainTCP
:Scope: input
:Type: boolean
:Default: input=0
:Required?: no
:Introduced: 3.11.6

Description
-----------
Permits the server to receive plain TCP syslog (without GSS protection) on the
same port that the GSS listener uses.

Input usage
-----------
.. _imgssapi.parameter.input.inputgssserverpermitplaintcp-usage:

.. code-block:: rsyslog

   module(load="imgssapi")
   $InputGSSServerPermitPlainTCP on

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imgssapi.parameter.legacy.inputgssserverpermitplaintcp:

- $InputGSSServerPermitPlainTCP — maps to InputGSSServerPermitPlainTCP (status: legacy)

.. index::
   single: imgssapi; $InputGSSServerPermitPlainTCP
   single: $InputGSSServerPermitPlainTCP

See also
--------
See also :doc:`../../configuration/modules/imgssapi`.
