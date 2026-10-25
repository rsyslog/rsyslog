.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Configure the omgssapi ActionGSSForwardDefaultTemplate legacy directive.
   :keywords: rsyslog, omgssapi, ActionGSSForwardDefaultTemplate, legacy configuration

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

This module supports legacy directives and selector actions. Load it before
setting the directive, then configure the forwarding action.

.. code-block:: none

   module(load="omgssapi")
   $ActionGSSForwardDefaultTemplate RSYSLOG_ForwardFormat
   *.* :omgssapi:receiver.mydomain.com

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The following directive is the supported configuration interface for omgssapi.

.. _omgssapi.parameter.legacy.actiongssforwarddefaulttemplate:

- ``$ActionGSSForwardDefaultTemplate`` — sets ActionGSSForwardDefaultTemplate for subsequent forwarding actions.

.. index::
   single: omgssapi; $ActionGSSForwardDefaultTemplate
   single: $ActionGSSForwardDefaultTemplate

See also
--------
See also :doc:`../../configuration/modules/omgssapi`.
