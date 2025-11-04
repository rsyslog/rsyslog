.. _param-omclickhouse-allowunsignedcerts:
.. _omclickhouse.parameter.module.allowunsignedcerts:

allowUnsignedCerts
==================

.. index::
   single: omclickhouse; allowUnsignedCerts
   single: allowUnsignedCerts

.. summary-start

Allows connections to servers that present unsigned TLS certificates.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: allowUnsignedCerts
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: not specified

Description
-----------
The module accepts connections to servers, which have unsigned certificates. If this parameter is disabled, the module will verify whether the certificates are authentic.

Module usage
------------
.. _param-omclickhouse-module-allowunsignedcerts:
.. _omclickhouse.parameter.module.allowunsignedcerts-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" allowUnsignedCerts="off")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
