.. _param-omclickhouse-allowunsignedcerts:
.. _omclickhouse.parameter.input.allowunsignedcerts:

allowUnsignedCerts
==================

.. index::
   single: omclickhouse; allowUnsignedCerts
   single: allowUnsignedCerts
   single: omclickhouse; allowunsignedcerts
   single: allowunsignedcerts

.. summary-start

Allows connections to servers that present unsigned TLS certificates.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: allowUnsignedCerts
:Scope: input
:Type: boolean
:Default: on
:Required?: no
:Introduced: not specified

Description
-----------
The module accepts connections to servers, which have unsigned certificates. If this parameter is disabled, the module will verify whether the certificates are authentic.

Input usage
-----------
.. _omclickhouse.parameter.input.allowunsignedcerts-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" allowUnsignedCerts="off")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
