.. _param-omdtls-template:
.. _omdtls.parameter.module.template:
.. _omdtls.parameter.input.template:

Template
========

.. index::
   single: omdtls; Template
   single: Template

.. summary-start

Selects the template used to format messages sent through omdtls actions.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: Template
:Scope: module, input
:Type: word
:Default: module=none, input=RSYSLOG_FileFormat
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Selects the template that formats messages for omdtls actions. If an action
does not specify this parameter, it uses ``RSYSLOG_FileFormat``. The module
parameter is defined for compatibility but currently ignored by actions.

Module usage
------------
.. _param-omdtls-module-template:
.. _omdtls.parameter.module.template-usage:

.. code-block:: rsyslog

   module(load="omdtls" template="ForwardingTemplate")

Input usage
-----------
.. _param-omdtls-input-template:
.. _omdtls.parameter.input.template-usage:

.. code-block:: rsyslog

   action(type="omdtls" target="192.0.2.1" template="ForwardingTemplate")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omdtls.parameter.legacy.actionforwarddefaulttemplatename:

- $ActionForwardDefaultTemplateName — maps to Template (status: legacy)

.. index::
   single: omdtls; $ActionForwardDefaultTemplateName
   single: $ActionForwardDefaultTemplateName

See also
--------
See also :doc:`../../configuration/modules/omdtls`.
