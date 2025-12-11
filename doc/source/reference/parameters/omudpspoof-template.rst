.. _param-omudpspoof-template:
.. _omudpspoof.parameter.module.template:

Template
========

.. index::
   single: omudpspoof; Template
   single: Template

.. summary-start
Sets the default template used by omudpspoof actions without an explicitly configured template.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/omudpspoof`.

:Name: Template
:Scope: module
:Type: word
:Default: RSYSLOG_TraditionalForwardFormat
:Required?: no
:Introduced: 5.1.3

Description
-----------
This setting instructs omudpspoof to use a template different from the default template for all of its actions that do not have a template specified explicitly.

Module usage
------------
.. _param-omudpspoof-module-template:
.. _omudpspoof.parameter.module.template-usage:

.. code-block:: rsyslog

   module(load="omudpspoof")
   action(type="omudpspoof" template="RSYSLOG_TraditionalForwardFormat" target="192.0.2.1")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omudpspoof.parameter.legacy.actionomudpspoofdefaulttemplate:
- $ActionOMUDPSpoofDefaultTemplate — maps to Template (status: legacy)

.. index::
   single: omudpspoof; $ActionOMUDPSpoofDefaultTemplate
   single: $ActionOMUDPSpoofDefaultTemplate

See also
--------
See also :doc:`../../configuration/modules/omudpspoof`.
