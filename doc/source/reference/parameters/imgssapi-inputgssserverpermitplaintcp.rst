.. _param-imgssapi-inputgssserverpermitplaintcp:
.. _imgssapi.parameter.input.inputgssserverpermitplaintcp:

InputGSSServerPermitPlainTcp
============================

.. index::
   single: imgssapi; InputGSSServerPermitPlainTcp
   single: InputGSSServerPermitPlainTcp

.. summary-start

Allows accepting plain TCP syslog traffic on the GSS-protected port.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imgssapi`.

:Name: InputGSSServerPermitPlainTcp
:Scope: input
:Type: boolean
:Default: 0
:Required?: no
:Introduced: 3.11.6

Description
-----------
Permits the server to receive plain TCP syslog (without GSS protection) on the
same port that the GSS listener uses.

.. warning::

   This creates an intentional security downgrade path. Use it only on trusted
   networks. Plain TCP fallback is limited to connections that are identified as
   regular octet-counted syslog before GSS token processing begins; malformed
   GSS tokens are no longer reinterpreted as plain TCP traffic.

Input usage
-----------
.. _imgssapi.parameter.input.inputgssserverpermitplaintcp-usage:

.. code-block:: rsyslog

   module(load="imgssapi")
   $inputGssServerPermitPlainTcp on

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _imgssapi.parameter.legacy.inputgssserverpermitplaintcp:

- $InputGSSServerPermitPlainTCP — maps to InputGSSServerPermitPlainTcp (status: legacy)
- $inputGssServerPermitPlainTcp — maps to InputGSSServerPermitPlainTcp (status: legacy)

.. index::
   single: imgssapi; $InputGSSServerPermitPlainTCP
   single: $InputGSSServerPermitPlainTCP
   single: imgssapi; $inputGssServerPermitPlainTcp
   single: $inputGssServerPermitPlainTcp

See also
--------
See also :doc:`../../configuration/modules/imgssapi`.
