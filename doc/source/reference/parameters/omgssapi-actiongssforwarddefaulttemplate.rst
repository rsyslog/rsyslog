.. _param-omgssapi-actiongssforwarddefaulttemplate:
.. _omgssapi.parameter.module.actiongssforwarddefaulttemplate:

ActionGSSForwardDefaultTemplate
===============================

.. index::
   single: omgssapi; ActionGSSForwardDefaultTemplate
   single: ActionGSSForwardDefaultTemplate

.. summary-start

Sets the default output template omgssapi applies when a forwarding action omits an explicit template.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omgssapi`.

:Name: ActionGSSForwardDefaultTemplate
:Scope: module
:Type: string
:Default: RSYSLOG_TraditionalForwardFormat
:Required?: no
:Introduced: 3.12.4

Description
-----------
Sets a new default template for the GSS-API forwarding action. When no template is
specified in the forwarding rule, omgssapi falls back to this value. By default the
module uses the built-in ``RSYSLOG_TraditionalForwardFormat`` template, matching the
legacy syslog forward format. Legacy syntax configures this with directives such as
``$ActionGSSForwardDefaultTemplate RSYSLOG_ForwardFormat``.

Module usage
------------
.. _param-omgssapi-module-actiongssforwarddefaulttemplate:
.. _omgssapi.parameter.module.actiongssforwarddefaulttemplate-usage:

.. code-block:: rsyslog

   module(
       load="omgssapi"
       actionGssForwardDefaultTemplate="RSYSLOG_ForwardFormat"
   )
   action(type="omgssapi" target="receiver.mydomain.com")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omgssapi.parameter.legacy.actiongssforwarddefaulttemplate:

- $ActionGSSForwardDefaultTemplate — maps to ActionGSSForwardDefaultTemplate (status: legacy)

.. index::
   single: omgssapi; $ActionGSSForwardDefaultTemplate
   single: $ActionGSSForwardDefaultTemplate

See also
--------
See also :doc:`../../configuration/modules/omgssapi`.
