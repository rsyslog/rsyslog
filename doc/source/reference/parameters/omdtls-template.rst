.. _param-omdtls-template:
.. _omdtls.parameter.module.template:
.. _omdtls.parameter.input.template:

template
========

.. index::
   single: omdtls; template
   single: template

.. summary-start

Selects the template used to format messages sent through omdtls actions.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: template
:Scope: module, input
:Type: word
:Default: module=none, input=RSYSLOG_FileFormat
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Selects the template that formats messages for omdtls actions. If an action
does not specify this parameter, it uses ``RSYSLOG_FileFormat``. The module
parameter is defined for compatibility but is currently ignored.

The module has always defaulted actions to ``RSYSLOG_FileFormat`` (see
``plugins/omdtls/omdtls.c``), even though earlier documentation mentioned
``RSYSLOG_TraditionalForwardFormat``. Update existing configurations only if
you require a different template.

Module usage
------------
.. _omdtls.parameter.module.template-usage:

.. code-block:: rsyslog

   module(load="omdtls" template="ForwardingTemplate")

Input usage
-----------
.. _omdtls.parameter.input.template-usage:

.. code-block:: rsyslog

   action(type="omdtls" target="192.0.2.1" template="ForwardingTemplate")

See also
--------
See also :doc:`../../configuration/modules/omdtls`.
