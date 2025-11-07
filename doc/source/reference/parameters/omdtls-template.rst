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
:Default: module=RSYSLOG_FileFormat, input=module parameter
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Sets a non-standard default template for this module. The same-named action
parameter overrides the module default for the individual action instance.

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
