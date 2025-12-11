.. _param-omrelp-template:
.. _omrelp.parameter.input.template:

Template
========

.. index::
   single: omrelp; Template
   single: Template

.. summary-start

Selects the output template used to format RELP messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: Template
:Scope: input
:Type: string
:Default: input=RSYSLOG_ForwardFormat
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Defines the template to be used for the output.

Input usage
-----------
.. _param-omrelp-input-template:
.. _omrelp.parameter.input.template-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" template="RSYSLOG_ForwardFormat")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
