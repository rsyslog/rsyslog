.. _param-omudpspoof-sourcetemplate:
.. _omudpspoof.parameter.module.sourcetemplate:

sourceTemplate
==============

.. index::
   single: omudpspoof; sourceTemplate
   single: sourceTemplate

.. summary-start
Names the template that provides the spoofed source IP address for sent messages.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/omudpspoof`.

:Name: sourceTemplate
:Scope: module
:Type: word
:Default: RSYSLOG_omudpspoofDfltSourceTpl
:Required?: no
:Introduced: 5.1.3

Description
-----------
This is the name of the template that contains a numerical IP address that is to be used as the source system IP address. While it may often be a constant value, it can be generated as usual via the property replacer, as long as it is a valid IPv4 address. If not specified, the built-in default template RSYSLOG_omudpspoofDfltSourceTpl is used. This template is defined as follows:

.. code-block:: none

   $template RSYSLOG_omudpspoofDfltSourceTpl,"%fromhost-ip%"

So in essence, the default template spoofs the address of the system the message was received from. This is considered the most important use case.

Module usage
------------
.. _param-omudpspoof-module-sourcetemplate:
.. _omudpspoof.parameter.module.sourcetemplate-usage:

.. code-block:: rsyslog

   template(name="spoofaddr" type="string" string="192.0.2.100")
   action(type="omudpspoof" target="192.0.2.1" sourceTemplate="spoofaddr")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omudpspoof.parameter.legacy.actionomudpspoofsourcenametemplate:
- $ActionOMUDPSpoofSourceNameTemplate — maps to sourceTemplate (status: legacy)

.. index::
   single: omudpspoof; $ActionOMUDPSpoofSourceNameTemplate
   single: $ActionOMUDPSpoofSourceNameTemplate

See also
--------
See also :doc:`../../configuration/modules/omudpspoof`.
