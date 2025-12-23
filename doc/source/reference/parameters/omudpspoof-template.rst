.. _param-omudpspoof-template:
.. _omudpspoof.parameter.module.template:
.. _omudpspoof.parameter.action.template:

template
========

.. index::
   single: omudpspoof; template
   single: template

.. summary-start
Sets the default template used by omudpspoof actions without an explicitly configured template.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/omudpspoof`.

:Name: template
:Scope: module, action
:Type: word
:Default: RSYSLOG_TraditionalForwardFormat
:Required?: no
:Introduced: 5.1.3

Description
-----------
This setting specifies the template for formatting output messages. When set at the module level, it becomes the default for all ``omudpspoof`` actions that do not have an explicit template. When set on a specific action, it overrides the module-level default for that action.

Module usage
------------
.. _param-omudpspoof-module-template:
.. _omudpspoof.parameter.module.template-usage:

.. code-block:: rsyslog

   module(load="omudpspoof" template="RSYSLOG_TraditionalForwardFormat")

Action usage
------------
.. _param-omudpspoof-action-template:
.. _omudpspoof.parameter.action.template-usage:

.. code-block:: rsyslog

   action(type="omudpspoof" template="RSYSLOG_TraditionalForwardFormat" target="192.0.2.1")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omudpspoof.parameter.legacy.actionomudpspoofdefaulttemplate:
- $ActionOMUDPSpoofDefaultTemplate — maps to template (status: legacy)

.. index::
   single: omudpspoof; $ActionOMUDPSpoofDefaultTemplate
   single: $ActionOMUDPSpoofDefaultTemplate

See also
--------
See also :doc:`../../configuration/modules/omudpspoof`.
