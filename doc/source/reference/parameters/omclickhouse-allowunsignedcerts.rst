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

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: allowUnsignedCerts
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: not specified

Description
-----------
If set to ``"on"``, the module accepts servers with unsigned certificates.
By default (``"off"``), TLS peer certificates are verified.

Input usage
-----------
.. _omclickhouse.parameter.input.allowunsignedcerts-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" allowUnsignedCerts="off")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
