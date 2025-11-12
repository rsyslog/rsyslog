.. _param-omgssapi-gssforwardservicename:
.. _omgssapi.parameter.module.gssforwardservicename:

GssForwardServiceName
=====================

.. index::
   single: omgssapi; GssForwardServiceName
   single: GssForwardServiceName

.. summary-start

Sets the Kerberos service principal base name used when omgssapi establishes a GSSAPI-secured forwarding session.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omgssapi`.

:Name: GssForwardServiceName
:Scope: module
:Type: string
:Default: host
:Required?: no
:Introduced: 1.21.2

Description
-----------
Specifies the service name used by the client when forwarding GSS-API wrapped messages.
If unset, the module uses ``host`` as the base name. The actual service principal
is constructed by appending ``@`` and the hostname that follows the ``:omgssapi:``
selector in legacy configurations. In legacy syntax this is configured with
``$GssForwardServiceName rsyslog``.

Module usage
------------
.. _param-omgssapi-module-gssforwardservicename:
.. _omgssapi.parameter.module.gssforwardservicename-usage:

.. code-block:: rsyslog

   module(load="omgssapi" gssForwardServiceName="rsyslog")
   action(type="omgssapi" target="receiver.mydomain.com")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omgssapi.parameter.legacy.gssforwardservicename:

- $GssForwardServiceName — maps to GssForwardServiceName (status: legacy)

.. index::
   single: omgssapi; $GssForwardServiceName
   single: $GssForwardServiceName

See also
--------
See also :doc:`../../configuration/modules/omgssapi`.
